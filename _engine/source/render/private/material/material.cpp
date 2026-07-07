#include "render/material/material.h"
#include "log/core/log_macros.h"
#include "core/allocator/allocator.h"
#include <cstring>
#include <memory>

namespace Entelechy
{

// ------------------------------------------------------------------
// std140 alignment helpers
// ------------------------------------------------------------------
static u32 alignOffset(u32 offset, u32 alignment)
{
    return (offset + alignment - 1) & ~(alignment - 1);
}

static u32 getParamAlign(MaterialParamType type)
{
    switch (type)
    {
    case MaterialParamType::Float:
        return 4;
    case MaterialParamType::Vec2:
        return 8;
    case MaterialParamType::Vec3:
        return 16;
    case MaterialParamType::Vec4:
        return 16;
    case MaterialParamType::Mat4:
        return 16;
    default:
        return 4;
    }
}

static u32 getParamSize(MaterialParamType type)
{
    switch (type)
    {
    case MaterialParamType::Float:
        return 4;
    case MaterialParamType::Vec2:
        return 8;
    case MaterialParamType::Vec3:
        return 16; // std140: vec3 padded to 16
    case MaterialParamType::Vec4:
        return 16;
    case MaterialParamType::Mat4:
        return 64;
    default:
        return 0;
    }
}

// ------------------------------------------------------------------
// Material
// ------------------------------------------------------------------
Material::Material() = default;

Material::~Material()
{
    shutdown();
}

Material::Material(Material &&other) noexcept
    : m_valid(other.m_valid),
      m_vertex_shader(std::move(other.m_vertex_shader)),
      m_fragment_shader(std::move(other.m_fragment_shader)),
      m_pipeline_desc(std::move(other.m_pipeline_desc)),
      m_pipeline_state(std::move(other.m_pipeline_state)),
      m_uniform_data(other.m_uniform_data),
      m_uniform_data_size(other.m_uniform_data_size),
      m_params(std::move(other.m_params)),
      m_textures(std::move(other.m_textures))
{
    other.m_valid = false;
    other.m_uniform_data = nullptr;
    other.m_uniform_data_size = 0;
}

Material &Material::operator=(Material &&other) noexcept
{
    if (this != &other)
    {
        shutdown();
        m_valid = other.m_valid;
        m_vertex_shader = std::move(other.m_vertex_shader);
        m_fragment_shader = std::move(other.m_fragment_shader);
        m_pipeline_desc = std::move(other.m_pipeline_desc);
        m_pipeline_state = std::move(other.m_pipeline_state);
        m_uniform_data = other.m_uniform_data;
        m_uniform_data_size = other.m_uniform_data_size;
        m_params = std::move(other.m_params);
        m_textures = std::move(other.m_textures);
        other.m_valid = false;
        other.m_uniform_data = nullptr;
        other.m_uniform_data_size = 0;
    }
    return *this;
}

bool Material::init(IRHIDevice *device, ShaderCache *shaderCache, const char *vertexSource, const char *fragmentSource,
                    const MaterialParamDesc *params, u32 paramCount, const PipelineStateDesc &pipelineDesc)
{
    shutdown();

    if (!device || !vertexSource || !fragmentSource)
    {
        LOG_ERROR(LogCategories::kEngine, "Material::init: invalid arguments");
        return false;
    }

    // Compile shaders
    usize vsLen = std::strlen(vertexSource);
    usize fsLen = std::strlen(fragmentSource);

    if (shaderCache)
    {
        m_vertex_shader = shaderCache->getOrCreateShader(device, ShaderStage::Vertex, vertexSource, vsLen);
        m_fragment_shader = shaderCache->getOrCreateShader(device, ShaderStage::Fragment, fragmentSource, fsLen);
    }
    else
    {
        m_vertex_shader = device->createShader(ShaderStage::Vertex, vertexSource, vsLen);
        m_fragment_shader = device->createShader(ShaderStage::Fragment, fragmentSource, fsLen);
    }

    if (!m_vertex_shader || !m_fragment_shader)
    {
        LOG_ERROR(LogCategories::kEngine, "Material::init: shader compilation failed");
        shutdown();
        return false;
    }

    // Create pipeline state
    m_pipeline_desc = pipelineDesc;
    m_pipeline_desc.vertexShader = m_vertex_shader.get();
    m_pipeline_desc.fragmentShader = m_fragment_shader.get();
    m_pipeline_state = device->createPipelineState(m_pipeline_desc);
    if (!m_pipeline_state)
    {
        LOG_ERROR(LogCategories::kEngine, "Material::init: pipeline state creation failed");
        shutdown();
        return false;
    }

    // Build parameter layout
    if (params && paramCount > 0)
    {
        u32 uniformOffset = 0;
        u32 nextTextureSlot = 0;

        for (u32 i = 0; i < paramCount; ++i)
        {
            StringId name = params[i].name;
            if (name.value() == 0)
                continue;

            StringId key = name;
            ParamSlot slot;
            slot.type = params[i].type;

            if (params[i].type == MaterialParamType::Texture)
            {
                slot.offset = 0;
                slot.textureSlot = nextTextureSlot++;
            }
            else
            {
                u32 align = getParamAlign(params[i].type);
                u32 size = getParamSize(params[i].type);
                uniformOffset = alignOffset(uniformOffset, align);
                slot.offset = uniformOffset;
                slot.textureSlot = 0;
                uniformOffset += size;
            }

            m_params.insert(key, slot);
        }

        if (uniformOffset > 0)
        {
            m_uniform_data_size = alignOffset(uniformOffset, 16);
            m_uniform_data = static_cast<u8 *>(DefaultAllocator::alloc(m_uniform_data_size, alignof(u8)));
            std::memset(m_uniform_data, 0, m_uniform_data_size);
        }
    }

    m_valid = true;
    return true;
}

void Material::shutdown()
{
    m_valid = false;
    m_pipeline_state.reset();
    m_vertex_shader.reset();
    m_fragment_shader.reset();
    if (m_uniform_data)
    {
        DefaultAllocator::free(m_uniform_data);
        m_uniform_data = nullptr;
    }
    m_uniform_data_size = 0;
    m_params.clear();
    m_textures.clear();
}

// ------------------------------------------------------------------
// Parameter setters
// ------------------------------------------------------------------
void Material::setFloat(StringId name, f32 value)
{
    if (!m_uniform_data || name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->type != MaterialParamType::Float)
        return;
    std::memcpy(m_uniform_data + slot->offset, &value, sizeof(f32));
}

void Material::setVec2(StringId name, const Vec2 &value)
{
    if (!m_uniform_data || name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->type != MaterialParamType::Vec2)
        return;
    std::memcpy(m_uniform_data + slot->offset, &value.x, 2 * sizeof(f32));
}

void Material::setVec3(StringId name, const Vec3 &value)
{
    if (!m_uniform_data || name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->type != MaterialParamType::Vec3)
        return;
    std::memcpy(m_uniform_data + slot->offset, &value.x, 3 * sizeof(f32));
    // Note: std140 vec3 has 4 floats (16 bytes), but we only write 3.
    // The shader only reads .xyz; the w component is padding.
}

void Material::setVec4(StringId name, const Vec4 &value)
{
    if (!m_uniform_data || name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->type != MaterialParamType::Vec4)
        return;
    std::memcpy(m_uniform_data + slot->offset, &value.x, 4 * sizeof(f32));
}

void Material::setMat4(StringId name, const Mat4 &value, bool /*transpose*/)
{
    if (!m_uniform_data || name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->type != MaterialParamType::Mat4)
        return;
    std::memcpy(m_uniform_data + slot->offset, value.m, 16 * sizeof(f32));
}

void Material::setTexture(StringId name, RHITextureRef texture)
{
    if (name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->type != MaterialParamType::Texture)
        return;
    m_textures.insert(name, texture);
}

// ------------------------------------------------------------------
// Bind
// ------------------------------------------------------------------
void Material::bind(IRHICommandList *cmdList)
{
    if (!m_valid || !cmdList)
        return;

    cmdList->bindPipeline(m_pipeline_state.get());

    // Upload scalar/vector/matrix uniforms
    for (auto kv : m_params)
    {
        StringId name = kv.first;
        const ParamSlot &slot = kv.second;

        switch (slot.type)
        {
        case MaterialParamType::Float:
        {
            f32 v;
            std::memcpy(&v, m_uniform_data + slot.offset, sizeof(f32));
            cmdList->setUniformFloat(name, v);
            break;
        }
        case MaterialParamType::Vec2:
        {
            cmdList->setUniformVec2(name, reinterpret_cast<f32 *>(m_uniform_data + slot.offset));
            break;
        }
        case MaterialParamType::Vec3:
        {
            cmdList->setUniformVec3(name, reinterpret_cast<f32 *>(m_uniform_data + slot.offset));
            break;
        }
        case MaterialParamType::Vec4:
        {
            cmdList->setUniformVec4(name, reinterpret_cast<f32 *>(m_uniform_data + slot.offset));
            break;
        }
        case MaterialParamType::Mat4:
        {
            cmdList->setUniformMat4(name, reinterpret_cast<f32 *>(m_uniform_data + slot.offset), false);
            break;
        }
        default:
            break;
        }
    }

    // Bind textures and set sampler uniforms
    for (auto kv : m_textures)
    {
        StringId name = kv.first;
        RHITexture *texture = kv.second.get();
        if (!texture)
            continue;

        auto *slot = m_params.find(name);
        if (!slot)
            continue;

        cmdList->bindTexture(slot->textureSlot, texture);
        cmdList->setUniformInt(name, static_cast<i32>(slot->textureSlot));
    }
}

} // namespace Entelechy
