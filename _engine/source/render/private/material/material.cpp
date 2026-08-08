#include "render/material/material.h"
#include "render/binding/constant_buffer_ring.h"
#include "log/core/log_macros.h"
#include "core/allocator/allocator.h"
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Entelechy
{

namespace
{

// Read a file into a byte buffer. Tries the path as-is first, then relative
// to the executable directory so shaders/reflection can be found regardless
// of the process working directory.
std::vector<u8> readFileResolved(const char *path)
{
    auto tryRead = [](const char *p) -> std::vector<u8> {
        std::ifstream f(p, std::ios::binary | std::ios::ate);
        if (!f.is_open())
            return {};
        auto size = f.tellg();
        if (size <= 0)
            return {};
        f.seekg(0, std::ios::beg);
        std::vector<u8> buf(static_cast<size_t>(size));
        f.read(reinterpret_cast<char *>(buf.data()), size);
        return buf;
    };

    auto data = tryRead(path);
    if (!data.empty())
        return data;

#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char *lastSep = strrchr(exePath, '\\');
    if (!lastSep)
        lastSep = strrchr(exePath, '/');
    if (lastSep)
    {
        *(lastSep + 1) = '\0';
        return tryRead((std::string(exePath) + path).c_str());
    }
#endif
    return {};
}

// Derive the reflection JSON path from a bytecode path:
// "shaders/pbr_lit_vertex.glsl" -> "shaders/pbr_lit_vertex_reflection.json"
std::string reflectionPathFor(const std::string &bytecodePath)
{
    const size_t dot = bytecodePath.find_last_of('.');
    const std::string base = dot == std::string::npos ? bytecodePath : bytecodePath.substr(0, dot);
    return base + "_reflection.json";
}

ShaderMemberType memberTypeForParamType(MaterialParamType type)
{
    switch (type)
    {
    case MaterialParamType::Float:
        return ShaderMemberType::Float;
    case MaterialParamType::Vec2:
        return ShaderMemberType::Vec2;
    case MaterialParamType::Vec3:
        return ShaderMemberType::Vec3;
    case MaterialParamType::Vec4:
        return ShaderMemberType::Vec4;
    case MaterialParamType::Mat3:
        return ShaderMemberType::Mat3;
    case MaterialParamType::Mat4:
        return ShaderMemberType::Mat4;
    default:
        return ShaderMemberType::Unknown;
    }
}

} // namespace

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
      m_cbuffers(std::move(other.m_cbuffers)),
      m_bind_layout(std::move(other.m_bind_layout)),
      m_bind_group(std::move(other.m_bind_group)),
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
        m_cbuffers = std::move(other.m_cbuffers);
        m_bind_layout = std::move(other.m_bind_layout);
        m_bind_group = std::move(other.m_bind_group);
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

void Material::insertCBuffer(const ShaderReflectionCBuffer &ref, ShaderStage stage)
{
    CBufferSlot slot;
    slot.binding = ref.binding;
    slot.size = ref.size;
    slot.stage = stage;

    usize insertAt = 0;
    while (insertAt < m_cbuffers.size() && m_cbuffers[insertAt].binding < slot.binding)
        ++insertAt;
    m_cbuffers.insert(insertAt, slot);
}

bool Material::buildParamLayout(const MaterialParamDesc *params, u32 paramCount,
                                const ShaderReflection &vsReflection, const ShaderReflection &psReflection)
{
    if (!params || paramCount == 0)
        return true;

    for (u32 i = 0; i < paramCount; ++i)
    {
        StringId key = StringInternPool::instance().intern(params[i].name);
        if (key.value() == 0)
            continue;

        ParamSlot slot;
        slot.memberType = ShaderMemberType::Unknown;

        if (params[i].type == MaterialParamType::Texture)
        {
            // Resolve the t-register from the (PS-first) texture lists.
            for (const ShaderReflection *ref : {&psReflection, &vsReflection})
            {
                for (usize t = 0; t < ref->textures.size(); ++t)
                {
                    if (ref->textures[t].name == params[i].name)
                    {
                        slot.isTexture = true;
                        slot.textureSlot = ref->textures[t].binding;
                        break;
                    }
                }
                if (slot.isTexture)
                    break;
            }
            if (slot.isTexture)
                m_params.insert(key, slot);
            continue;
        }

        const ShaderMemberType wantType = memberTypeForParamType(params[i].type);

        for (const ShaderReflection *ref : {&vsReflection, &psReflection})
        {
            for (usize c = 0; c < ref->cbuffers.size(); ++c)
            {
                const ShaderReflectionCBuffer &cbuf = ref->cbuffers[c];
                const ShaderReflectionMember *member = cbuf.findMember(params[i].name);
                if (!member)
                    continue;

                if (member->type != wantType)
                {
                    LOG_WARN(LogCategories::kEngine,
                                "Material: param '%s' type mismatch (shader member has different type)", params[i].name);
                    break;
                }

                for (usize mi = 0; mi < m_cbuffers.size(); ++mi)
                {
                    if (m_cbuffers[mi].binding == cbuf.binding)
                    {
                        slot.cbufferIndex = static_cast<u32>(mi);
                        break;
                    }
                }
                slot.memberOffset = member->offset;
                slot.memberType = member->type;
                m_params.insert(key, slot);
                break;
            }
            if (slot.memberType != ShaderMemberType::Unknown)
                break;
        }
    }
    return true;
}

bool Material::initFromBytecode(IRHIDevice *device,
                                const char *vertexBytecodePath, const char *fragmentBytecodePath,
                                ShaderBytecodeFormat format,
                                const MaterialParamDesc *params, u32 paramCount,
                                const PipelineStateDesc &pipelineDesc)
{
    shutdown();

    if (!device || !vertexBytecodePath || !fragmentBytecodePath)
    {
        LOG_ERROR(LogCategories::kEngine, "Material::initFromBytecode: invalid arguments");
        return false;
    }

    // -- Read bytecode -----------------------------------------------------
    auto vsData = readFileResolved(vertexBytecodePath);
    auto fsData = readFileResolved(fragmentBytecodePath);

    if (vsData.empty())
    {
        LOG_ERROR(LogCategories::kEngine, "Material::initFromBytecode: cannot read vertex shader: %s",
                  vertexBytecodePath);
        return false;
    }
    if (fsData.empty())
    {
        LOG_ERROR(LogCategories::kEngine, "Material::initFromBytecode: cannot read fragment shader: %s",
                  fragmentBytecodePath);
        return false;
    }

    // -- Load reflection metadata (cbuffer layout, texture bindings) -------
    ShaderReflection vsReflection;
    ShaderReflection psReflection;
    {
        const std::string vsReflectionPath = reflectionPathFor(vertexBytecodePath);
        auto vsJson = readFileResolved(vsReflectionPath.c_str());
        if (vsJson.empty() ||
            !parseShaderReflection(reinterpret_cast<const char *>(vsJson.data()), vsJson.size(), vsReflection))
        {
            LOG_ERROR(LogCategories::kEngine, "Material::initFromBytecode: cannot load reflection: %s",
                      vsReflectionPath.c_str());
            return false;
        }
        const std::string psReflectionPath = reflectionPathFor(fragmentBytecodePath);
        auto psJson = readFileResolved(psReflectionPath.c_str());
        if (psJson.empty() ||
            !parseShaderReflection(reinterpret_cast<const char *>(psJson.data()), psJson.size(), psReflection))
        {
            LOG_ERROR(LogCategories::kEngine, "Material::initFromBytecode: cannot load reflection: %s",
                      psReflectionPath.c_str());
            return false;
        }
    }

    // -- Create shaders ----------------------------------------------------
    ShaderBytecode vsBytecode{};
    vsBytecode.stage = ShaderStage::Vertex;
    vsBytecode.format = format;
    vsBytecode.data = vsData.data();
    vsBytecode.size = vsData.size();
    vsBytecode.entryPoint = "main";

    ShaderBytecode fsBytecode{};
    fsBytecode.stage = ShaderStage::Fragment;
    fsBytecode.format = format;
    fsBytecode.data = fsData.data();
    fsBytecode.size = fsData.size();
    fsBytecode.entryPoint = "main";

    m_vertex_shader = device->createShader(vsBytecode);
    m_fragment_shader = device->createShader(fsBytecode);

    if (!m_vertex_shader || !m_fragment_shader)
    {
        LOG_ERROR(LogCategories::kEngine, "Material::initFromBytecode: shader creation failed");
        shutdown();
        return false;
    }

    // -- Create pipeline state ---------------------------------------------
    m_pipeline_desc = pipelineDesc;
    m_pipeline_desc.vertexShader = m_vertex_shader.get();
    m_pipeline_desc.fragmentShader = m_fragment_shader.get();
    m_pipeline_state = device->createPipelineState(m_pipeline_desc);
    if (!m_pipeline_state)
    {
        LOG_ERROR(LogCategories::kEngine, "Material::initFromBytecode: pipeline state creation failed");
        shutdown();
        return false;
    }

    // -- Merge cbuffers (binding-sorted) + bind group layout ---------------
    for (usize c = 0; c < vsReflection.cbuffers.size(); ++c)
        insertCBuffer(vsReflection.cbuffers[c], ShaderStage::Vertex);
    for (usize c = 0; c < psReflection.cbuffers.size(); ++c)
        insertCBuffer(psReflection.cbuffers[c], ShaderStage::Fragment);

    u32 blobOffset = 0;
    for (usize i = 0; i < m_cbuffers.size(); ++i)
    {
        m_cbuffers[i].blobOffset = blobOffset;
        blobOffset += m_cbuffers[i].size;

        BindGroupLayoutEntry entry;
        entry.binding = m_cbuffers[i].binding;
        entry.type = BindResourceType::UniformBuffer;
        entry.stage = m_cbuffers[i].stage;
        m_bind_layout.addEntry(entry);
    }
    for (usize c = 0; c < psReflection.textures.size(); ++c)
    {
        BindGroupLayoutEntry entry;
        entry.binding = psReflection.textures[c].binding;
        entry.type = BindResourceType::Texture;
        entry.stage = ShaderStage::Fragment;
        m_bind_layout.addEntry(entry);
    }
    for (usize c = 0; c < vsReflection.textures.size(); ++c)
    {
        BindGroupLayoutEntry entry;
        entry.binding = vsReflection.textures[c].binding;
        entry.type = BindResourceType::Texture;
        entry.stage = ShaderStage::Vertex;
        m_bind_layout.addEntry(entry);
    }
    if (!m_bind_group.init(&m_bind_layout))
    {
        LOG_ERROR(LogCategories::kEngine, "Material::initFromBytecode: bind group init failed");
        shutdown();
        return false;
    }

    // -- CPU constant blob + param table ------------------------------------
    m_uniform_data_size = blobOffset;
    if (m_uniform_data_size > 0)
    {
        m_uniform_data = static_cast<u8 *>(DefaultAllocator::alloc(m_uniform_data_size, 16));
        std::memset(m_uniform_data, 0, m_uniform_data_size);
    }

    if (!buildParamLayout(params, paramCount, vsReflection, psReflection))
    {
        LOG_ERROR(LogCategories::kEngine, "Material::initFromBytecode: param layout build failed");
        shutdown();
        return false;
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
    m_bind_group.shutdown();
    m_bind_layout = BindGroupLayout{};
    m_cbuffers.clear();
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
// Parameter setters — resolve the shader member name, write into the
// CPU blob at (cbuffer blob offset + member offset).
// ------------------------------------------------------------------
void Material::setFloat(StringId name, f32 value)
{
    if (!m_uniform_data || name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->isTexture || slot->memberType != ShaderMemberType::Float)
        return;
    std::memcpy(m_uniform_data + m_cbuffers[slot->cbufferIndex].blobOffset + slot->memberOffset, &value, sizeof(f32));
}

void Material::setVec2(StringId name, const Vec2 &value)
{
    if (!m_uniform_data || name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->isTexture || slot->memberType != ShaderMemberType::Vec2)
        return;
    std::memcpy(m_uniform_data + m_cbuffers[slot->cbufferIndex].blobOffset + slot->memberOffset, &value.x,
                2 * sizeof(f32));
}

void Material::setVec3(StringId name, const Vec3 &value)
{
    if (!m_uniform_data || name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->isTexture || slot->memberType != ShaderMemberType::Vec3)
        return;
    std::memcpy(m_uniform_data + m_cbuffers[slot->cbufferIndex].blobOffset + slot->memberOffset, &value.x,
                3 * sizeof(f32));
}

void Material::setVec4(StringId name, const Vec4 &value)
{
    if (!m_uniform_data || name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->isTexture || slot->memberType != ShaderMemberType::Vec4)
        return;
    std::memcpy(m_uniform_data + m_cbuffers[slot->cbufferIndex].blobOffset + slot->memberOffset, &value.x,
                4 * sizeof(f32));
}

void Material::setMat3(StringId name, const Mat3 &value)
{
    // No engine shader uses mat3 cbuffer members (matrices are float4x4 to
    // keep HLSL/std140 layouts identical); kept for API completeness. Writes
    // the std140 layout (3 columns of vec3, each 16 bytes) — HLSL packs mat3
    // with 12-byte columns, so a mat3 member would NOT be cross-backend
    // safe; do not add one without revisiting this setter.
    if (!m_uniform_data || name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->isTexture || slot->memberType != ShaderMemberType::Mat3)
        return;
    u8 *dst = m_uniform_data + m_cbuffers[slot->cbufferIndex].blobOffset + slot->memberOffset;
    for (int col = 0; col < 3; ++col)
    {
        std::memcpy(dst + col * 16, value.m + col * 3, 3 * sizeof(f32));
        std::memset(dst + col * 16 + 12, 0, 4);
    }
}

void Material::setMat4(StringId name, const Mat4 &value)
{
    // Writes the matrix column-major (Mat4::m). The GLSL UBO declares the
    // matrix `layout(row_major)` and SPIRV-Cross emits the multiply in the
    // transposed operand order, so column-major bytes are what both the GL
    // and D3D12 backends expect from one shared blob.
    if (!m_uniform_data || name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || slot->isTexture || slot->memberType != ShaderMemberType::Mat4)
        return;
    std::memcpy(m_uniform_data + m_cbuffers[slot->cbufferIndex].blobOffset + slot->memberOffset, value.m,
                16 * sizeof(f32));
}

void Material::setTexture(StringId name, RHITextureRef texture)
{
    if (name.value() == 0)
        return;
    auto *slot = m_params.find(name);
    if (!slot || !slot->isTexture)
        return;
    m_textures.insert(name, texture);
}

// ------------------------------------------------------------------
// Bind
// ------------------------------------------------------------------
void Material::bind(IRHICommandList *cmdList, ConstantBufferRing *ring)
{
    if (!m_valid || !cmdList || !ring)
        return;

    cmdList->bindPipeline(m_pipeline_state.get());

    // Upload each cbuffer into the ring and point the BindGroup at the
    // freshly allocated block. Textures are set on the group during prepare
    // (setTexture) and re-asserted here each draw.
    for (usize i = 0; i < m_cbuffers.size(); ++i)
    {
        const CBufferSlot &slot = m_cbuffers[i];
        u32 ringOffset = 0;
        if (!ring->allocate(m_uniform_data + slot.blobOffset, slot.size, ringOffset))
            continue; // ring overflow — draw will use stale data (logged once)
        m_bind_group.setBuffer(slot.binding, ring->buffer(), ringOffset, slot.size);
    }
    for (auto kv : m_textures)
    {
        RHITexture *texture = kv.second.get();
        if (!texture)
            continue;
        const ParamSlot *slot = m_params.find(kv.first);
        if (!slot)
            continue;
        m_bind_group.setTexture(slot->textureSlot, texture);
    }

    m_bind_group.bind(cmdList);
}

} // namespace Entelechy
