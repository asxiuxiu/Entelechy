#include "material.h"
#include "log/log_macros.h"
#include <cstring>

namespace Entelechy {

// ------------------------------------------------------------------
// std140 alignment helpers
// ------------------------------------------------------------------
static u32 alignOffset(u32 offset, u32 alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}

static u32 getParamAlign(MaterialParamType type) {
    switch (type) {
        case MaterialParamType::Float: return 4;
        case MaterialParamType::Vec2:  return 8;
        case MaterialParamType::Vec3:  return 16;
        case MaterialParamType::Vec4:  return 16;
        case MaterialParamType::Mat4:  return 16;
        default:                       return 4;
    }
}

static u32 getParamSize(MaterialParamType type) {
    switch (type) {
        case MaterialParamType::Float: return 4;
        case MaterialParamType::Vec2:  return 8;
        case MaterialParamType::Vec3:  return 16; // std140: vec3 padded to 16
        case MaterialParamType::Vec4:  return 16;
        case MaterialParamType::Mat4:  return 64;
        default:                       return 0;
    }
}

// ------------------------------------------------------------------
// Material
// ------------------------------------------------------------------
Material::Material() = default;

Material::~Material() {
    shutdown();
}

Material::Material(Material&& other) noexcept
    : m_valid(other.m_valid)
    , m_vertexShader(std::move(other.m_vertexShader))
    , m_fragmentShader(std::move(other.m_fragmentShader))
    , m_pipelineDesc(std::move(other.m_pipelineDesc))
    , m_pipelineState(std::move(other.m_pipelineState))
    , m_uniformData(other.m_uniformData)
    , m_uniformDataSize(other.m_uniformDataSize)
    , m_params(std::move(other.m_params))
    , m_textures(std::move(other.m_textures)) {
    other.m_valid = false;
    other.m_uniformData = nullptr;
    other.m_uniformDataSize = 0;
}

Material& Material::operator=(Material&& other) noexcept {
    if (this != &other) {
        shutdown();
        m_valid = other.m_valid;
        m_vertexShader = std::move(other.m_vertexShader);
        m_fragmentShader = std::move(other.m_fragmentShader);
        m_pipelineDesc = std::move(other.m_pipelineDesc);
        m_pipelineState = std::move(other.m_pipelineState);
        m_uniformData = other.m_uniformData;
        m_uniformDataSize = other.m_uniformDataSize;
        m_params = std::move(other.m_params);
        m_textures = std::move(other.m_textures);
        other.m_valid = false;
        other.m_uniformData = nullptr;
        other.m_uniformDataSize = 0;
    }
    return *this;
}

bool Material::init(IRHIDevice* device, ShaderCache* shaderCache,
                    const char* vertexSource, const char* fragmentSource,
                    const MaterialParamDesc* params, u32 paramCount,
                    const PipelineStateDesc& pipelineDesc) {
    shutdown();

    if (!device || !vertexSource || !fragmentSource) {
        LOG_ERROR(LogCategories::kEngine, "Material::init: invalid arguments");
        return false;
    }

    // Compile shaders
    usize vsLen = std::strlen(vertexSource);
    usize fsLen = std::strlen(fragmentSource);

    if (shaderCache) {
        m_vertexShader   = shaderCache->getOrCreateShader(device, ShaderStage::Vertex,   vertexSource,   vsLen);
        m_fragmentShader = shaderCache->getOrCreateShader(device, ShaderStage::Fragment, fragmentSource, fsLen);
    } else {
        m_vertexShader   = device->createShader(ShaderStage::Vertex,   vertexSource,   vsLen);
        m_fragmentShader = device->createShader(ShaderStage::Fragment, fragmentSource, fsLen);
    }

    if (!m_vertexShader || !m_fragmentShader) {
        LOG_ERROR(LogCategories::kEngine, "Material::init: shader compilation failed");
        shutdown();
        return false;
    }

    // Create pipeline state
    m_pipelineDesc = pipelineDesc;
    m_pipelineDesc.vertexShader = m_vertexShader.get();
    m_pipelineDesc.fragmentShader = m_fragmentShader.get();
    m_pipelineState = device->createPipelineState(m_pipelineDesc);
    if (!m_pipelineState) {
        LOG_ERROR(LogCategories::kEngine, "Material::init: pipeline state creation failed");
        shutdown();
        return false;
    }

    // Build parameter layout
    if (params && paramCount > 0) {
        u32 uniformOffset = 0;
        u32 nextTextureSlot = 0;

        for (u32 i = 0; i < paramCount; ++i) {
            const char* name = params[i].name;
            if (!name || name[0] == '\0') continue;

            SmallString key(name);
            ParamSlot slot;
            slot.type = params[i].type;

            if (params[i].type == MaterialParamType::Texture) {
                slot.offset = 0;
                slot.textureSlot = nextTextureSlot++;
            } else {
                u32 align = getParamAlign(params[i].type);
                u32 size  = getParamSize(params[i].type);
                uniformOffset = alignOffset(uniformOffset, align);
                slot.offset = uniformOffset;
                slot.textureSlot = 0;
                uniformOffset += size;
            }

            m_params.insert(key, slot);
        }

        if (uniformOffset > 0) {
            m_uniformDataSize = alignOffset(uniformOffset, 16);
            m_uniformData = new u8[m_uniformDataSize];
            std::memset(m_uniformData, 0, m_uniformDataSize);
        }
    }

    m_valid = true;
    return true;
}

void Material::shutdown() {
    m_valid = false;
    m_pipelineState.reset();
    m_vertexShader.reset();
    m_fragmentShader.reset();
    if (m_uniformData) {
        delete[] m_uniformData;
        m_uniformData = nullptr;
    }
    m_uniformDataSize = 0;
    m_params.clear();
    m_textures.clear();
}

// ------------------------------------------------------------------
// Parameter setters
// ------------------------------------------------------------------
void Material::setFloat(const char* name, f32 value) {
    if (!m_uniformData || !name) return;
    auto* slot = m_params.find(SmallString(name));
    if (!slot || slot->type != MaterialParamType::Float) return;
    std::memcpy(m_uniformData + slot->offset, &value, sizeof(f32));
}

void Material::setVec2(const char* name, const Vec2& value) {
    if (!m_uniformData || !name) return;
    auto* slot = m_params.find(SmallString(name));
    if (!slot || slot->type != MaterialParamType::Vec2) return;
    std::memcpy(m_uniformData + slot->offset, &value.x, 2 * sizeof(f32));
}

void Material::setVec3(const char* name, const Vec3& value) {
    if (!m_uniformData || !name) return;
    auto* slot = m_params.find(SmallString(name));
    if (!slot || slot->type != MaterialParamType::Vec3) return;
    std::memcpy(m_uniformData + slot->offset, &value.x, 3 * sizeof(f32));
    // Note: std140 vec3 has 4 floats (16 bytes), but we only write 3.
    // The shader only reads .xyz; the w component is padding.
}

void Material::setVec4(const char* name, const Vec4& value) {
    if (!m_uniformData || !name) return;
    auto* slot = m_params.find(SmallString(name));
    if (!slot || slot->type != MaterialParamType::Vec4) return;
    std::memcpy(m_uniformData + slot->offset, &value.x, 4 * sizeof(f32));
}

void Material::setMat4(const char* name, const Mat4& value, bool /*transpose*/) {
    if (!m_uniformData || !name) return;
    auto* slot = m_params.find(SmallString(name));
    if (!slot || slot->type != MaterialParamType::Mat4) return;
    std::memcpy(m_uniformData + slot->offset, value.m, 16 * sizeof(f32));
}

void Material::setTexture(const char* name, RHITextureRef texture) {
    if (!name) return;
    auto* slot = m_params.find(SmallString(name));
    if (!slot || slot->type != MaterialParamType::Texture) return;
    m_textures.insert(SmallString(name), texture);
}

// ------------------------------------------------------------------
// Bind
// ------------------------------------------------------------------
void Material::bind(IRHICommandList* cmdList) {
    if (!m_valid || !cmdList) return;

    cmdList->bindPipeline(m_pipelineState.get());

    // Upload scalar/vector/matrix uniforms
    for (auto kv : m_params) {
        const SmallString& name = kv.first;
        const ParamSlot& slot = kv.second;

        switch (slot.type) {
            case MaterialParamType::Float: {
                f32 v;
                std::memcpy(&v, m_uniformData + slot.offset, sizeof(f32));
                cmdList->setUniformFloat(name.c_str(), v);
                break;
            }
            case MaterialParamType::Vec2: {
                cmdList->setUniformVec2(name.c_str(), reinterpret_cast<f32*>(m_uniformData + slot.offset));
                break;
            }
            case MaterialParamType::Vec3: {
                cmdList->setUniformVec3(name.c_str(), reinterpret_cast<f32*>(m_uniformData + slot.offset));
                break;
            }
            case MaterialParamType::Vec4: {
                cmdList->setUniformVec4(name.c_str(), reinterpret_cast<f32*>(m_uniformData + slot.offset));
                break;
            }
            case MaterialParamType::Mat4: {
                cmdList->setUniformMat4(name.c_str(), reinterpret_cast<f32*>(m_uniformData + slot.offset), false);
                break;
            }
            default:
                break;
        }
    }

    // Bind textures and set sampler uniforms
    for (auto kv : m_textures) {
        const SmallString& name = kv.first;
        RHITexture* texture = kv.second.get();
        if (!texture) continue;

        auto* slot = m_params.find(name);
        if (!slot) continue;

        cmdList->bindTexture(slot->textureSlot, texture);
        cmdList->setUniformInt(name.c_str(), static_cast<i32>(slot->textureSlot));
    }
}

} // namespace Entelechy
