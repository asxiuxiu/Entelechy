#include "render_system/prepare/PrepareAssetsSystem.h"
#include "render_system/components/RenderComponents.h"
#include "asset/type/mesh_primitives.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"
#include "log/core/log_macros.h"
#include <cstddef>

namespace Entelechy
{

namespace
{

constexpr LogCategory kLogPrepare("Render");

// Unlit textured shader pair. The shader always samples; materials without
// a texture bind the 1x1 white fallback so uColor passes through unchanged.
// uAlphaCutoff implements AlphaMode::Mask (discard below cutoff); opaque
// materials pass 0 which disables the test. NOTE: the mask branch is
// unverified — Sponza has no mask materials to exercise it (Phase 4b).
const char *s_vertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char *s_fragmentShader = R"(#version 330 core
in vec2 vUV;
uniform vec3 uColor;
uniform float uAlphaCutoff;
uniform sampler2D uBaseColorTex;
out vec4 FragColor;
void main() {
    vec4 tex = texture(uBaseColorTex, vUV);
    if (uAlphaCutoff > 0.0 && tex.a < uAlphaCutoff)
        discard;
    FragColor = vec4(uColor * tex.rgb, 1.0);
}
)";

// Attribute layout of the interleaved MeshVertex stream.
const VertexAttributeDesc s_meshAttrs[] = {
    {0, 3, false, static_cast<u32>(offsetof(MeshVertex, position))},
    {1, 3, false, static_cast<u32>(offsetof(MeshVertex, normal))},
    {2, 2, false, static_cast<u32>(offsetof(MeshVertex, uv))},
    // tangent + tangentW are contiguous -> one vec4 attribute
    {3, 4, false, static_cast<u32>(offsetof(MeshVertex, tangent))},
};
constexpr u32 s_meshAttrCount = 4;

template <typename T>
bool loggedBefore(const DynamicArray<Handle<T>> &list, Handle<T> handle)
{
    for (usize i = 0; i < list.size(); ++i)
    {
        if (list[i] == handle)
            return true;
    }
    return false;
}

} // namespace

PrepareAssetsSystem::~PrepareAssetsSystem()
{
    if (m_initialized)
        shutdown();
}

bool PrepareAssetsSystem::init(IRHIDevice *device, ShaderCache *shaderCache)
{
    if (m_initialized)
        return true;
    if (!device || !shaderCache)
    {
        LOG_ERROR(kLogPrepare, "PrepareAssetsSystem: init requires a device and shader cache");
        return false;
    }
    m_device = device;
    m_shader_cache = shaderCache;

    // 1x1 white texture bound by materials without a base color texture.
    const u8 whitePixel[4] = {255, 255, 255, 255};
    TextureDesc whiteDesc{};
    whiteDesc.width = 1;
    whiteDesc.height = 1;
    whiteDesc.format = TextureFormat::RGBA8_UNORM;
    whiteDesc.usage = TextureUsage::Sampled;
    m_white_texture = m_device->createTexture(whiteDesc, whitePixel);
    if (!m_white_texture)
    {
        LOG_ERROR(kLogPrepare, "PrepareAssetsSystem: failed to create white fallback texture");
        return false;
    }

    // Fallback mesh: unit cube.
    const MeshAsset cube = buildCubeMesh(0.5f);
    if (!uploadMesh(cube, m_fallback_mesh))
    {
        LOG_ERROR(kLogPrepare, "PrepareAssetsSystem: failed to upload fallback cube");
        return false;
    }

    // Fallback material: unlit magenta, doubles as the "asset pending" marker.
    MaterialParamDesc params[] = {
        {"uMVP", MaterialParamType::Mat4},
        {"uColor", MaterialParamType::Vec3},
        {"uAlphaCutoff", MaterialParamType::Float},
        {"uBaseColorTex", MaterialParamType::Texture},
    };
    PipelineStateDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangles;
    pipelineDesc.rasterizerState.cullMode = CullMode::Back;
    pipelineDesc.depthStencilState.depthTest = true;
    pipelineDesc.depthStencilState.depthWrite = true;

    if (!m_fallback_material.material.init(m_device, m_shader_cache, s_vertexShader, s_fragmentShader, params, 4,
                                           pipelineDesc))
    {
        LOG_ERROR(kLogPrepare, "PrepareAssetsSystem: failed to init fallback material");
        return false;
    }
    m_fallback_material.material.setVec3("uColor"_sid, Vec3{1.0f, 0.0f, 1.0f});
    m_fallback_material.material.setFloat("uAlphaCutoff"_sid, 0.0f);
    m_fallback_material.material.setTexture("uBaseColorTex"_sid, m_white_texture);

    m_initialized = true;
    LOG_INFO(kLogPrepare, "PrepareAssetsSystem initialized (fallback cube + pink material ready)");
    return true;
}

void PrepareAssetsSystem::shutdown()
{
    if (!m_initialized)
        return;

    for (auto [handle, prepared] : m_materials)
    {
        (void)handle;
        prepared.material.shutdown();
    }
    m_materials.clear();
    m_meshes.clear();
    m_textures.clear();
    m_fallback_material.material.shutdown();
    m_fallback_mesh = PreparedMesh{};
    m_white_texture.reset();

    m_device = nullptr;
    m_shader_cache = nullptr;
    m_mesh_assets = nullptr;
    m_material_assets = nullptr;
    m_texture_assets = nullptr;
    m_initialized = false;
}

void PrepareAssetsSystem::bindAssets(Assets<MeshAsset> *meshes, Assets<MaterialAsset> *materials,
                                     Assets<TextureAsset> *textures)
{
    m_mesh_assets = meshes;
    m_material_assets = materials;
    m_texture_assets = textures;
}

bool PrepareAssetsSystem::uploadMesh(const MeshAsset &asset, PreparedMesh &out)
{
    if (asset.vertices.size() == 0 || asset.indices.size() == 0)
        return false;

    BufferDesc vbDesc{};
    vbDesc.size = static_cast<u32>(asset.vertices.size() * sizeof(MeshVertex));
    vbDesc.usage = BufferUsage::Vertex;
    vbDesc.vertexStride = MeshAsset::vertexStride();
    vbDesc.vertexAttributes = s_meshAttrs;
    vbDesc.vertexAttributeCount = s_meshAttrCount;

    out.vbo = m_device->createBuffer(vbDesc, asset.vertices.data());
    if (!out.vbo)
        return false;

    BufferDesc ibDesc{};
    ibDesc.size = static_cast<u32>(asset.indices.size() * sizeof(u32));
    ibDesc.usage = BufferUsage::Index;

    out.ibo = m_device->createBuffer(ibDesc, asset.indices.data());
    if (!out.ibo)
        return false;

    out.index_count = static_cast<u32>(asset.indices.size());
    return true;
}

bool PrepareAssetsSystem::prepareMesh(Handle<MeshAsset> handle)
{
    if (m_meshes.find(handle))
        return true;

    const MeshAsset *asset = m_mesh_assets ? m_mesh_assets->get(handle) : nullptr;
    if (!asset)
    {
        if (!loggedBefore(m_pending_meshes_logged, handle))
        {
            m_pending_meshes_logged.pushBack(handle);
            LOG_INFO(kLogPrepare, "Prepare: mesh %u not loaded yet, using fallback cube", handle.index);
        }
        return false;
    }

    PreparedMesh prepared;
    if (!uploadMesh(*asset, prepared))
    {
        LOG_ERROR(kLogPrepare, "Prepare: failed to upload mesh %u", handle.index);
        return false;
    }
    m_meshes.insert(handle, std::move(prepared));
    LOG_INFO(kLogPrepare, "Prepare: mesh %u uploaded (%u indices)", handle.index,
             m_meshes.find(handle)->index_count);
    return true;
}

RHITextureRef PrepareAssetsSystem::prepareTexture(Handle<TextureAsset> handle)
{
    if (RHITextureRef *existing = m_textures.find(handle))
        return *existing;

    const TextureAsset *asset = m_texture_assets ? m_texture_assets->get(handle) : nullptr;
    if (!asset || !asset->valid())
        return nullptr;

    TextureDesc desc{};
    desc.width = asset->width;
    desc.height = asset->height;
    desc.format = TextureFormat::RGBA8_UNORM;
    desc.usage = TextureUsage::Sampled;

    RHITextureRef texture = m_device->createTexture(desc, asset->pixels.data());
    if (!texture)
    {
        LOG_ERROR(kLogPrepare, "Prepare: failed to create texture %u (%ux%u)", handle.index, asset->width,
                  asset->height);
        return nullptr;
    }
    m_textures.insert(handle, texture);
    LOG_INFO(kLogPrepare, "Prepare: texture %u uploaded (%ux%u)", handle.index, asset->width, asset->height);
    return texture;
}

bool PrepareAssetsSystem::prepareMaterial(Handle<MaterialAsset> handle)
{
    if (m_materials.find(handle))
        return true;

    const MaterialAsset *asset = m_material_assets ? m_material_assets->get(handle) : nullptr;
    if (!asset)
    {
        if (!loggedBefore(m_pending_logged, handle))
        {
            m_pending_logged.pushBack(handle);
            LOG_INFO(kLogPrepare, "Prepare: material %u not loaded yet, using pink fallback", handle.index);
        }
        return false;
    }

    // Phase 4b: a texture path without a Handle means the spawn-side
    // backfill has not issued the texture load yet — stay pending instead
    // of preparing against the white texture (prepared materials are
    // never rebuilt, so preparing here would stick on white).
    if (asset->base_color_texture_path.length() > 0 && !asset->base_color_texture.valid())
    {
        if (!loggedBefore(m_pending_logged, handle))
        {
            m_pending_logged.pushBack(handle);
            LOG_INFO(kLogPrepare, "Prepare: material %u waiting for texture handle backfill, using pink fallback",
                     handle.index);
        }
        return false;
    }

    RHITextureRef texture = m_white_texture;
    if (asset->base_color_texture.valid())
    {
        texture = prepareTexture(asset->base_color_texture);
        if (!texture)
        {
            if (!loggedBefore(m_pending_logged, handle))
            {
                m_pending_logged.pushBack(handle);
                LOG_INFO(kLogPrepare, "Prepare: material %u waiting for texture %u, using pink fallback",
                         handle.index, asset->base_color_texture.index);
            }
            return false;
        }
    }

    MaterialParamDesc params[] = {
        {"uMVP", MaterialParamType::Mat4},
        {"uColor", MaterialParamType::Vec3},
        {"uAlphaCutoff", MaterialParamType::Float},
        {"uBaseColorTex", MaterialParamType::Texture},
    };
    PipelineStateDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangles;
    // D3 (Phase 4b): hand-written pipeline variant — double-sided materials
    // disable backface culling; no generic PSO variant system yet.
    pipelineDesc.rasterizerState.cullMode = asset->double_sided ? CullMode::None : CullMode::Back;
    pipelineDesc.depthStencilState.depthTest = true;
    pipelineDesc.depthStencilState.depthWrite = true;

    PreparedMaterial prepared;
    if (!prepared.material.init(m_device, m_shader_cache, s_vertexShader, s_fragmentShader, params, 4, pipelineDesc))
    {
        LOG_ERROR(kLogPrepare, "Prepare: failed to init material %u", handle.index);
        return false;
    }
    prepared.material.setVec3("uColor"_sid, asset->base_color);
    // AlphaMode::Mask discards below the cutoff; opaque and blend-as-opaque
    // (D3, correct blending is Phase 5+) pass 0 to disable the test.
    prepared.material.setFloat("uAlphaCutoff"_sid,
                               asset->alpha_mode == AlphaMode::Mask ? asset->alpha_cutoff : 0.0f);
    prepared.material.setTexture("uBaseColorTex"_sid, texture);

    m_materials.insert(handle, std::move(prepared));
    LOG_INFO(kLogPrepare, "Prepare: material %u ready (textured=%d double_sided=%d alpha_mode=%d)", handle.index,
             asset->base_color_texture.valid() ? 1 : 0, asset->double_sided ? 1 : 0,
             static_cast<int>(asset->alpha_mode));
    return true;
}

void PrepareAssetsSystem::run(World &renderWorld)
{
    if (!m_initialized)
        return;

    m_stats.pending_meshes = 0;
    m_stats.pending_materials = 0;

    ConstQuery<RenderMesh> meshQuery(renderWorld);
    for (auto [entity, mesh] : meshQuery)
    {
        (void)entity;
        if (!prepareMesh(mesh->mesh_asset_id))
            ++m_stats.pending_meshes;
    }

    ConstQuery<RenderMaterial> materialQuery(renderWorld);
    for (auto [entity, material] : materialQuery)
    {
        (void)entity;
        if (!prepareMaterial(material->material_asset_id))
            ++m_stats.pending_materials;
    }

    m_stats.prepared_meshes = static_cast<u32>(m_meshes.size());
    m_stats.prepared_materials = static_cast<u32>(m_materials.size());
}

PreparedMesh *PrepareAssetsSystem::findMesh(Handle<MeshAsset> handle)
{
    return m_meshes.find(handle);
}

PreparedMaterial *PrepareAssetsSystem::findMaterial(Handle<MaterialAsset> handle)
{
    return m_materials.find(handle);
}

} // namespace Entelechy
