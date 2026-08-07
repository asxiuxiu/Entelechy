#include "render_system/prepare/prepare_assets_system.h"
#include "render_system/components/render_components.h"
#include "render/rhi/rhi_device_factory.h"
#include "asset/type/mesh_primitives.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"
#include "log/core/log_macros.h"
#include <cstddef>
#include <cstring>
#include <string>

namespace Entelechy
{

namespace
{

constexpr LogCategory kLogPrepare("Render");

// Lit PBR shader pair: single directional light + constant ambient, Lambert
// diffuse + GGX Cook-Torrance specular, approximate gamma (pow 2.2 in, pow
// 1/2.2 out — not true sRGB, see TODO.md). The shader always samples;
// materials without a texture bind the 1x1 white fallback so uColor passes
// through unchanged. uAlphaCutoff implements AlphaMode::Mask (discard below
// cutoff); opaque materials pass 0 which disables the test. NOTE: the mask
// branch is unverified — Sponza has no mask materials to exercise it.
// The vs outputs a Gram-Schmidt orthogonalized world-space TBN
// (B reconstructed in the fs as cross(N,T) * tangentW handedness); the fs
// perturbs N with the tangent-space normal map and multiplies the
// metallic/roughness factors with the MR texture (glTF: G=roughness,
// B=metallic). Materials without those textures take factor-only branches
// gated by uHasNormalTex/uHasMRTex (hand-written branches, no variant
// system).
const char *s_vertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent; // xyz = tangent, w = handedness (+/-1)
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;
out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vTangent;
out float vTangentW;
out vec2 vUV;
void main() {
    vWorldPos = (uModel * vec4(aPos, 1.0)).xyz;
    // N goes through the normal matrix (inverse-transpose),
    // T through the plain model matrix, then T is re-orthogonalized
    // against N (Gram-Schmidt) — B is rebuilt in the fragment shader.
    vec3 N = uNormalMatrix * aNormal;
    vec3 T = mat3(uModel) * aTangent.xyz;
    T = normalize(T - dot(T, N) * N);
    vNormal = N;
    vTangent = T;
    vTangentW = aTangent.w;
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char *s_fragmentShader = R"(#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vTangent;
in float vTangentW;
in vec2 vUV;
uniform vec3 uColor;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAlphaCutoff;
uniform sampler2D uBaseColorTex;
uniform sampler2D uNormalTex;
uniform sampler2D uMRTex;
uniform float uHasNormalTex;
uniform float uHasMRTex;
uniform vec3 uViewPos;
uniform vec3 uLightDir;       // direction the light travels
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform float uAmbient;
out vec4 FragColor;

const float PI = 3.14159265359;

// D: GGX / Trowbridge-Reitz.
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// G: Schlick-GGX (direct-lighting k).
float geometrySchlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

// F: Schlick approximation.
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 tex = texture(uBaseColorTex, vUV);
    if (uAlphaCutoff > 0.0 && tex.a < uAlphaCutoff)
        discard;

    vec3 albedo = pow(uColor * tex.rgb, vec3(2.2));
    float metallic = uMetallic;
    float roughness = uRoughness;
    if (uHasMRTex > 0.5) {
        // glTF metallicRoughnessTexture: G = roughness, B = metallic,
        // multiplied with the factors.
        vec3 mr = texture(uMRTex, vUV).rgb;
        metallic *= mr.b;
        roughness *= mr.g;
    }
    metallic = clamp(metallic, 0.0, 1.0);
    roughness = clamp(roughness, 0.05, 1.0);

    vec3 N = normalize(vNormal);
    if (uHasNormalTex > 0.5) {
        // B = cross(N,T) * handedness; the normal map is
        // tangent-space, so N = TBN * (tex*2-1).
        vec3 T = normalize(vTangent - dot(vTangent, N) * N);
        vec3 B = cross(N, T) * vTangentW;
        N = normalize(mat3(T, B, N) * (texture(uNormalTex, vUV).xyz * 2.0 - 1.0));
    }
    if (!gl_FrontFacing)
        N = -N; // double-sided materials: flip normals on backfaces
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(V + L);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NdotL = max(dot(N, L), 0.0);
    vec3 radiance = uLightColor * uLightIntensity;

    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;
    vec3 color = uAmbient * albedo + Lo;

    FragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
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

// Resolve "shaders/<base>" to the precompiled bytecode file for the
// device's backend (e.g. shaders/pbr_lit_vertex.dxil on D3D12).
std::string shaderPathForBackend(IRHIDevice *device, const char *base)
{
    return std::string("shaders/") + base + shaderFileExtensionForBackend(device->getBackendType());
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

    // Material parameter layout matching the HLSL cbuffer structure.
    // After SPIRV-Cross flattening, cbuffers become vec4 arrays:
    //   PerFrame[3]:  [0]=uViewPos.xyz+pad, [1]=uLightDir.xyz+uLightIntensity, [2]=uLightColor.xyz+uAmbient
    //   PerMaterial[2]: [0]=uColor.xyz+uMetallic, [1]=uRoughness+uAlphaCutoff+uHasNormalTex+uHasMRTex
    //   PerDraw[11]:  [0..3]=uMVP rows, [4..7]=uModel rows, [8..10]=uNormalMatrix (mat3 packed in 3 vec4)
    // Textures keep their original HLSL names — the shader compiler renames
    // SPIRV-Cross combined samplers back to the source texture names.
    MaterialParamDesc params[] = {
        // PerFrame uniforms (flattened to type_PerFrame[N])
        {"type_PerFrame[0]", MaterialParamType::Vec4},  // uViewPos.xyz + pad
        {"type_PerFrame[1]", MaterialParamType::Vec4},  // uLightDir.xyz + uLightIntensity
        {"type_PerFrame[2]", MaterialParamType::Vec4},  // uLightColor.xyz + uAmbient
        // PerMaterial uniforms (flattened to type_PerMaterial[N])
        {"type_PerMaterial[0]", MaterialParamType::Vec4}, // uColor.xyz + uMetallic
        {"type_PerMaterial[1]", MaterialParamType::Vec4}, // uRoughness + uAlphaCutoff + uHasNormalTex + uHasMRTex
        // PerDraw uniforms (flattened to type_PerDraw[N])
        {"type_PerDraw[0]", MaterialParamType::Vec4},  // uMVP row 0
        {"type_PerDraw[1]", MaterialParamType::Vec4},  // uMVP row 1
        {"type_PerDraw[2]", MaterialParamType::Vec4},  // uMVP row 2
        {"type_PerDraw[3]", MaterialParamType::Vec4},  // uMVP row 3
        {"type_PerDraw[4]", MaterialParamType::Vec4},  // uModel row 0
        {"type_PerDraw[5]", MaterialParamType::Vec4},  // uModel row 1
        {"type_PerDraw[6]", MaterialParamType::Vec4},  // uModel row 2
        {"type_PerDraw[7]", MaterialParamType::Vec4},  // uModel row 3
        {"type_PerDraw[8]", MaterialParamType::Vec4},  // uNormalMatrix row 0
        {"type_PerDraw[9]", MaterialParamType::Vec4},  // uNormalMatrix row 1
        {"type_PerDraw[10]", MaterialParamType::Vec4}, // uNormalMatrix row 2
        // Textures (combined samplers renamed to original HLSL names)
        {"uBaseColorTex", MaterialParamType::Texture},
        {"uNormalTex", MaterialParamType::Texture},
        {"uMRTex", MaterialParamType::Texture},
    };
    constexpr u32 s_materialParamCount = 19;
    PipelineStateDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangles;
    pipelineDesc.rasterizerState.cullMode = CullMode::Back;
    pipelineDesc.depthStencilState.depthTest = true;
    pipelineDesc.depthStencilState.depthWrite = true;
    pipelineDesc.vertexStride = MeshAsset::vertexStride();
    std::memcpy(pipelineDesc.vertexAttributes, s_meshAttrs, sizeof(s_meshAttrs));
    pipelineDesc.vertexAttributeCount = s_meshAttrCount;

    const std::string pbrVs = shaderPathForBackend(m_device, "pbr_lit_vertex");
    const std::string pbrPs = shaderPathForBackend(m_device, "pbr_lit_pixel");
    if (!m_fallback_material.material.initFromBytecode(m_device, pbrVs.c_str(), pbrPs.c_str(),
                                                       shaderFormatForBackend(m_device->getBackendType()), params,
                                                       s_materialParamCount, pipelineDesc))
    {
        LOG_ERROR(kLogPrepare, "PrepareAssetsSystem: failed to init fallback material");
        return false;
    }
    // Set fallback material uniforms using flattened cbuffer layout.
    // PerMaterial[0] = {uColor.xyz, uMetallic}, PerMaterial[1] = {uRoughness, uAlphaCutoff, uHasNormalTex, uHasMRTex}
    m_fallback_material.material.setVec4("type_PerMaterial[0]"_sid, Vec4{1.0f, 0.0f, 1.0f, 0.0f}); // color + metallic
    m_fallback_material.material.setVec4("type_PerMaterial[1]"_sid, Vec4{0.9f, 0.0f, 0.0f, 0.0f}); // roughness + alpha + flags
    m_fallback_material.material.setTexture("uBaseColorTex"_sid, m_white_texture);
    m_fallback_material.material.setTexture("uNormalTex"_sid, m_white_texture);
    m_fallback_material.material.setTexture("uMRTex"_sid, m_white_texture);

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
    // Full mip chain: without mipmaps, minified 4K textures alias badly
    // (visible as high-frequency speckle, worst on normal maps).
    u32 mip_extent = desc.width > desc.height ? desc.width : desc.height;
    while (mip_extent > 1)
    {
        mip_extent >>= 1;
        ++desc.mipLevels;
    }

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

    // A texture path without a Handle means the spawn-side
    // backfill has not issued the texture load yet — stay pending instead
    // of preparing against the white texture (prepared materials are
    // never rebuilt, so preparing here would stick on white).
    // Same guard for the normal/MR textures.
    if ((asset->base_color_texture_path.length() > 0 && !asset->base_color_texture.valid()) ||
        (asset->normal_texture_path.length() > 0 && !asset->normal_texture.valid()) ||
        (asset->mr_texture_path.length() > 0 && !asset->mr_texture.valid()))
    {
        if (!loggedBefore(m_pending_logged, handle))
        {
            m_pending_logged.pushBack(handle);
            LOG_INFO(kLogPrepare, "Prepare: material %u waiting for texture handle backfill, using pink fallback",
                     handle.index);
        }
        return false;
    }

    // A material stays pending (pink fallback) until every texture it
    // references has been uploaded; the prepared material then binds the
    // real textures — the same "not ready -> fallback -> hot-swap"
    // mechanism baseColor already used (now extended to normal/MR).
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

    RHITextureRef normalTexture = m_white_texture;
    const bool hasNormalTexture = asset->normal_texture.valid();
    if (hasNormalTexture)
    {
        normalTexture = prepareTexture(asset->normal_texture);
        if (!normalTexture)
        {
            if (!loggedBefore(m_pending_logged, handle))
            {
                m_pending_logged.pushBack(handle);
                LOG_INFO(kLogPrepare, "Prepare: material %u waiting for texture %u, using pink fallback",
                         handle.index, asset->normal_texture.index);
            }
            return false;
        }
    }

    RHITextureRef mrTexture = m_white_texture;
    const bool hasMrTexture = asset->mr_texture.valid();
    if (hasMrTexture)
    {
        mrTexture = prepareTexture(asset->mr_texture);
        if (!mrTexture)
        {
            if (!loggedBefore(m_pending_logged, handle))
            {
                m_pending_logged.pushBack(handle);
                LOG_INFO(kLogPrepare, "Prepare: material %u waiting for texture %u, using pink fallback",
                         handle.index, asset->mr_texture.index);
            }
            return false;
        }
    }

    // Material parameter layout matching the HLSL cbuffer structure.
    // After SPIRV-Cross flattening, cbuffers become vec4 arrays:
    //   PerFrame[3]:   [0]=uViewPos.xyz+pad, [1]=uLightDir.xyz+uLightIntensity, [2]=uLightColor.xyz+uAmbient
    //   PerMaterial[2]: [0]=uColor.xyz+uMetallic, [1]=uRoughness+uAlphaCutoff+uHasNormalTex+uHasMRTex
    //   PerDraw[11]:   [0..3]=uMVP rows, [4..7]=uModel rows, [8..10]=uNormalMatrix (mat3 in 3 vec4)
    // Textures keep their original HLSL names — the shader compiler renames
    // SPIRV-Cross combined samplers back to the source texture names.
    MaterialParamDesc params[] = {
        // PerFrame uniforms (flattened to type_PerFrame[N])
        {"type_PerFrame[0]", MaterialParamType::Vec4},  // uViewPos.xyz + pad
        {"type_PerFrame[1]", MaterialParamType::Vec4},  // uLightDir.xyz + uLightIntensity
        {"type_PerFrame[2]", MaterialParamType::Vec4},  // uLightColor.xyz + uAmbient
        // PerMaterial uniforms (flattened to type_PerMaterial[N])
        {"type_PerMaterial[0]", MaterialParamType::Vec4}, // uColor.xyz + uMetallic
        {"type_PerMaterial[1]", MaterialParamType::Vec4}, // uRoughness + uAlphaCutoff + uHasNormalTex + uHasMRTex
        // PerDraw uniforms (flattened to type_PerDraw[N])
        {"type_PerDraw[0]", MaterialParamType::Vec4},  // uMVP row 0
        {"type_PerDraw[1]", MaterialParamType::Vec4},  // uMVP row 1
        {"type_PerDraw[2]", MaterialParamType::Vec4},  // uMVP row 2
        {"type_PerDraw[3]", MaterialParamType::Vec4},  // uMVP row 3
        {"type_PerDraw[4]", MaterialParamType::Vec4},  // uModel row 0
        {"type_PerDraw[5]", MaterialParamType::Vec4},  // uModel row 1
        {"type_PerDraw[6]", MaterialParamType::Vec4},  // uModel row 2
        {"type_PerDraw[7]", MaterialParamType::Vec4},  // uModel row 3
        {"type_PerDraw[8]", MaterialParamType::Vec4},  // uNormalMatrix row 0
        {"type_PerDraw[9]", MaterialParamType::Vec4},  // uNormalMatrix row 1
        {"type_PerDraw[10]", MaterialParamType::Vec4}, // uNormalMatrix row 2
        // Textures (combined samplers renamed to original HLSL names)
        {"uBaseColorTex", MaterialParamType::Texture},
        {"uNormalTex", MaterialParamType::Texture},
        {"uMRTex", MaterialParamType::Texture},
    };
    constexpr u32 s_materialParamCount = 19;
    PipelineStateDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangles;
    // Hand-written pipeline variant — double-sided materials
    // disable backface culling; no generic PSO variant system yet.
    pipelineDesc.rasterizerState.cullMode = asset->double_sided ? CullMode::None : CullMode::Back;
    pipelineDesc.depthStencilState.depthTest = true;
    pipelineDesc.depthStencilState.depthWrite = true;
    pipelineDesc.vertexStride = MeshAsset::vertexStride();
    std::memcpy(pipelineDesc.vertexAttributes, s_meshAttrs, sizeof(s_meshAttrs));
    pipelineDesc.vertexAttributeCount = s_meshAttrCount;

    PreparedMaterial prepared;
    const std::string pbrVs = shaderPathForBackend(m_device, "pbr_lit_vertex");
    const std::string pbrPs = shaderPathForBackend(m_device, "pbr_lit_pixel");
    if (!prepared.material.initFromBytecode(m_device, pbrVs.c_str(), pbrPs.c_str(),
                                            shaderFormatForBackend(m_device->getBackendType()), params,
                                            s_materialParamCount, pipelineDesc))
    {
        LOG_ERROR(kLogPrepare, "Prepare: failed to init material %u", handle.index);
        return false;
    }
    // Set material uniforms using flattened cbuffer layout.
    // PerMaterial[0] = {uColor.xyz, uMetallic}, PerMaterial[1] = {uRoughness, uAlphaCutoff, uHasNormalTex, uHasMRTex}
    f32 alphaCutoff = asset->alpha_mode == AlphaMode::Mask ? asset->alpha_cutoff : 0.0f;
    prepared.material.setVec4("type_PerMaterial[0]"_sid,
                              Vec4{asset->base_color.x, asset->base_color.y, asset->base_color.z, asset->metallic_factor});
    prepared.material.setVec4("type_PerMaterial[1]"_sid,
                              Vec4{asset->roughness_factor, alphaCutoff,
                                   hasNormalTexture ? 1.0f : 0.0f, hasMrTexture ? 1.0f : 0.0f});
    prepared.material.setTexture("uBaseColorTex"_sid, texture);
    prepared.material.setTexture("uNormalTex"_sid, normalTexture);
    prepared.material.setTexture("uMRTex"_sid, mrTexture);

    m_materials.insert(handle, std::move(prepared));
    LOG_INFO(kLogPrepare, "Prepare: material %u ready (textured=%d normal=%d mr=%d double_sided=%d alpha_mode=%d)",
             handle.index, asset->base_color_texture.valid() ? 1 : 0, hasNormalTexture ? 1 : 0,
             hasMrTexture ? 1 : 0, asset->double_sided ? 1 : 0, static_cast<int>(asset->alpha_mode));
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
