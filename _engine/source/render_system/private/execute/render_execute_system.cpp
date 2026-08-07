#include "render_system/execute/render_execute_system.h"
#include "render_system/prepare/prepare_assets_system.h"
#include "render_system/components/render_camera.h"
#include "render_system/components/render_components.h"
#include "render_system/components/render_light.h"
#include "render_system/components/render_sky.h"
#include "render_system/phase/render_resources.h"
#include "render/rhi/gl_rhi_device.h"
#include "render/rhi/rhi_device_factory.h"
#include "render/material/shader_cache.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"
#include "log/core/log_macros.h"
#include "core/math/mat3.h"
#include <string>

namespace Entelechy
{

namespace
{

// Sky gradient shader pair: a fullscreen triangle drawn right
// after clear and before the opaque phase. The vs pins the triangle to the
// far plane (NDC z = 1) and reconstructs the world-space far-plane position
// via the inverse view-projection; the fs normalizes the view ray and blends
// horizon -> zenith along its y component. Output uses the same approximate
// gamma as the lit PBR shader (pow(1/2.2), not true sRGB).
const char *s_skyVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aNDC;
uniform mat4 uInvViewProj;
out vec3 vFarPos;
void main() {
    gl_Position = vec4(aNDC, 1.0, 1.0);
    vec4 world = uInvViewProj * vec4(aNDC, 1.0, 1.0);
    vFarPos = world.xyz / world.w;
}
)";

const char *s_skyFragmentShader = R"(#version 330 core
in vec3 vFarPos;
uniform vec3 uViewPos;
uniform vec3 uHorizonColor;
uniform vec3 uZenithColor;
out vec4 FragColor;
void main() {
    vec3 dir = normalize(vFarPos - uViewPos);
    float t = clamp(dir.y, 0.0, 1.0);
    vec3 color = mix(uHorizonColor, uZenithColor, t);
    FragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
)";

} // namespace

RenderExecuteSystem::RenderExecuteSystem() = default;

RenderExecuteSystem::~RenderExecuteSystem()
{
    if (m_initialized)
        shutdown();
}

bool RenderExecuteSystem::init(IRHIDevice *device)
{
    if (m_initialized)
        return true;

    if (!device)
    {
        LOG_ERROR(LogCategories::kEngine, "RenderExecuteSystem: null device");
        return false;
    }
    m_device = device;

    m_shader_cache = std::make_unique<ShaderCache>();

    // Sky gradient pass. Best-effort: failure only disables
    // the sky, the rest of the pipeline keeps working.
    m_sky_ready = initSkyPass();
    if (!m_sky_ready)
    {
        LOG_ERROR(LogCategories::kEngine, "RenderExecuteSystem: sky pass init failed, sky disabled");
    }

    m_initialized = true;
    LOG_INFO(LogCategories::kEngine, "RenderExecuteSystem initialized (RHI + ShaderCache)");
    return true;
}

void RenderExecuteSystem::shutdown()
{
    if (!m_initialized)
        return;

    m_sky_material.shutdown();
    m_sky_vbo.reset();
    m_sky_ready = false;
    m_shader_cache.reset();
    // Device is borrowed — do not shut it down or delete it here.
    m_device = nullptr;
    m_initialized = false;
}

IRHIDevice *RenderExecuteSystem::device()
{
    return m_device;
}

ShaderCache *RenderExecuteSystem::shaderCache()
{
    return m_shader_cache.get();
}

usize RenderExecuteSystem::psoCacheSize() const
{
    // PSO cache size is only available on the GL backend. Other backends
    // will need their own query path once they exist.
    if (!m_device || m_device->getBackendType() != RenderBackendType::OpenGL)
        return 0;
    auto *glDevice = static_cast<GLRHIDevice *>(m_device);
    return glDevice->getPSOManager().getCacheSize();
}

bool RenderExecuteSystem::initSkyPass()
{
    // Fullscreen triangle in NDC, oversized so 3 vertices cover the viewport.
    const f32 vertices[] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    const VertexAttributeDesc attrs[] = {{0, 2, false, 0}};

    BufferDesc vbDesc{};
    vbDesc.size = sizeof(vertices);
    vbDesc.usage = BufferUsage::Vertex;
    vbDesc.vertexStride = sizeof(f32) * 2;
    vbDesc.vertexAttributes = attrs;
    vbDesc.vertexAttributeCount = 1;
    m_sky_vbo = m_device->createBuffer(vbDesc, vertices);
    if (!m_sky_vbo)
        return false;

    // Sky shader flattened cbuffers (VS and PS use distinct block names so
    // their flattened uniforms don't collide in the shared GL namespace):
    // Vertex: type_PerFrame[0..3]   = uInvViewProj (mat4 as 4 vec4 rows)
    // Pixel:  type_PerFramePS[0..2] = {uViewPos, uHorizonColor, uZenithColor}
    MaterialParamDesc params[] = {
        {"type_PerFrame[0]", MaterialParamType::Vec4},
        {"type_PerFrame[1]", MaterialParamType::Vec4},
        {"type_PerFrame[2]", MaterialParamType::Vec4},
        {"type_PerFrame[3]", MaterialParamType::Vec4},
        {"type_PerFramePS[0]", MaterialParamType::Vec4},
        {"type_PerFramePS[1]", MaterialParamType::Vec4},
        {"type_PerFramePS[2]", MaterialParamType::Vec4},
    };
    constexpr u32 s_skyParamCount = 7;
    PipelineStateDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangles;
    pipelineDesc.rasterizerState.cullMode = CullMode::None;
    // The triangle sits at NDC z = 1 against the freshly cleared depth
    // buffer, so LessEqual passes everywhere the scene has not drawn yet;
    // depth write stays off and the opaque phase overwrites the sky normally.
    pipelineDesc.depthStencilState.depthTest = true;
    pipelineDesc.depthStencilState.depthWrite = false;
    pipelineDesc.depthStencilState.depthFunc = CompareFunc::LessEqual;
    pipelineDesc.vertexStride = sizeof(f32) * 2;
    pipelineDesc.vertexAttributes[0] = {0, 2, false, 0};
    pipelineDesc.vertexAttributeCount = 1;

    const std::string skyVs = std::string("shaders/sky_vertex") + shaderFileExtensionForBackend(m_device->getBackendType());
    const std::string skyPs = std::string("shaders/sky_pixel") + shaderFileExtensionForBackend(m_device->getBackendType());
    if (!m_sky_material.initFromBytecode(m_device, skyVs.c_str(), skyPs.c_str(),
                                         shaderFormatForBackend(m_device->getBackendType()), params, s_skyParamCount,
                                         pipelineDesc))
        return false;

    // Set default sky colors using flattened cbuffer layout
    m_sky_material.setVec4("type_PerFramePS[1]"_sid, Vec4{0.55f, 0.65f, 0.75f, 0.0f}); // horizon
    m_sky_material.setVec4("type_PerFramePS[2]"_sid, Vec4{0.10f, 0.23f, 0.55f, 0.0f}); // zenith
    return true;
}

void RenderExecuteSystem::drawSky(const ExtractedView &view, const ExtractedSky &sky, IRHICommandList *cmdList)
{
    // Vertex stage: type_PerFrame[0..3] = uInvViewProj (mat4 rows)
    Mat4 invVP = (view.proj_matrix * view.view_matrix).inverse();
    m_sky_material.setVec4("type_PerFrame[0]"_sid, Vec4{invVP.m[0], invVP.m[1], invVP.m[2], invVP.m[3]});
    m_sky_material.setVec4("type_PerFrame[1]"_sid, Vec4{invVP.m[4], invVP.m[5], invVP.m[6], invVP.m[7]});
    m_sky_material.setVec4("type_PerFrame[2]"_sid, Vec4{invVP.m[8], invVP.m[9], invVP.m[10], invVP.m[11]});
    m_sky_material.setVec4("type_PerFrame[3]"_sid, Vec4{invVP.m[12], invVP.m[13], invVP.m[14], invVP.m[15]});
    // Pixel stage: type_PerFramePS[0] = uViewPos, [1] = horizon, [2] = zenith
    m_sky_material.setVec4("type_PerFramePS[0]"_sid, Vec4{view.view_pos.x, view.view_pos.y, view.view_pos.z, 0.0f});
    m_sky_material.setVec4("type_PerFramePS[1]"_sid,
                           Vec4{sky.horizon_color.x, sky.horizon_color.y, sky.horizon_color.z, 0.0f});
    m_sky_material.setVec4("type_PerFramePS[2]"_sid,
                           Vec4{sky.zenith_color.x, sky.zenith_color.y, sky.zenith_color.z, 0.0f});
    m_sky_material.bind(cmdList);
    cmdList->bindVertexBuffer(m_sky_vbo.get(), 0, 0);
    cmdList->draw(3, 0);
    // The sky pass is view-level, not a queued phase item — keep it out of
    // m_stats.draw_calls so draw_calls == visible stays meaningful.
}

void RenderExecuteSystem::drawItem(World &renderWorld, const ExtractedView &view, const ExtractedLight &light,
                                   Entity renderEntity, IRHICommandList *cmdList, PrepareAssetsSystem &prepare)
{
    const RenderTransform *transform = renderWorld.getComponent<RenderTransform>(renderEntity);
    const RenderMesh *mesh = renderWorld.getComponent<RenderMesh>(renderEntity);
    if (!transform || !mesh)
    {
        ++m_stats.skipped_missing_mesh;
        return;
    }

    const PreparedMesh *gpuMesh = prepare.findMesh(mesh->mesh_asset_id);
    if (!gpuMesh)
    {
        gpuMesh = prepare.fallbackMesh();
        ++m_stats.fallback_mesh_draws;
    }

    const RenderMaterial *materialRef = renderWorld.getComponent<RenderMaterial>(renderEntity);
    PreparedMaterial *prepared = materialRef ? prepare.findMaterial(materialRef->material_asset_id) : nullptr;
    if (!prepared)
    {
        prepared = prepare.fallbackMaterial();
        ++m_stats.fallback_material_draws;
    }

    // Per-draw uniforms using flattened cbuffer layout.
    // PerFrame[0] = {uViewPos.xyz, pad}, [1] = {uLightDir.xyz, uLightIntensity}, [2] = {uLightColor.xyz, uAmbient}
    // PerDraw[0..3] = uMVP rows, [4..7] = uModel rows, [8..10] = uNormalMatrix rows (mat3 in 3 vec4)
    prepared->material.setVec4("type_PerFrame[0]"_sid, Vec4{view.view_pos.x, view.view_pos.y, view.view_pos.z, 0.0f});
    prepared->material.setVec4("type_PerFrame[1]"_sid, Vec4{light.direction.x, light.direction.y, light.direction.z, light.intensity});
    prepared->material.setVec4("type_PerFrame[2]"_sid, Vec4{light.color.x, light.color.y, light.color.z, light.ambient});

    Mat4 mvp = view.proj_matrix * view.view_matrix * transform->world_matrix;
    prepared->material.setVec4("type_PerDraw[0]"_sid, Vec4{mvp.m[0], mvp.m[1], mvp.m[2], mvp.m[3]});
    prepared->material.setVec4("type_PerDraw[1]"_sid, Vec4{mvp.m[4], mvp.m[5], mvp.m[6], mvp.m[7]});
    prepared->material.setVec4("type_PerDraw[2]"_sid, Vec4{mvp.m[8], mvp.m[9], mvp.m[10], mvp.m[11]});
    prepared->material.setVec4("type_PerDraw[3]"_sid, Vec4{mvp.m[12], mvp.m[13], mvp.m[14], mvp.m[15]});

    const Mat4 &model = transform->world_matrix;
    prepared->material.setVec4("type_PerDraw[4]"_sid, Vec4{model.m[0], model.m[1], model.m[2], model.m[3]});
    prepared->material.setVec4("type_PerDraw[5]"_sid, Vec4{model.m[4], model.m[5], model.m[6], model.m[7]});
    prepared->material.setVec4("type_PerDraw[6]"_sid, Vec4{model.m[8], model.m[9], model.m[10], model.m[11]});
    prepared->material.setVec4("type_PerDraw[7]"_sid, Vec4{model.m[12], model.m[13], model.m[14], model.m[15]});

    Mat3 normalMat = Mat3::normalMatrix(model);
    prepared->material.setVec4("type_PerDraw[8]"_sid, Vec4{normalMat.m[0], normalMat.m[1], normalMat.m[2], 0.0f});
    prepared->material.setVec4("type_PerDraw[9]"_sid, Vec4{normalMat.m[3], normalMat.m[4], normalMat.m[5], 0.0f});
    prepared->material.setVec4("type_PerDraw[10]"_sid, Vec4{normalMat.m[6], normalMat.m[7], normalMat.m[8], 0.0f});

    prepared->material.bind(cmdList);

    cmdList->bindVertexBuffer(gpuMesh->vbo.get(), 0, 0);
    cmdList->bindIndexBuffer(gpuMesh->ibo.get(), 0);
    cmdList->drawIndexed(gpuMesh->index_count, 0, 0);
    ++m_stats.draw_calls;
}

void RenderExecuteSystem::run(World &renderWorld, PrepareAssetsSystem &prepare)
{
    m_stats = ExecuteStats{};
    if (!m_initialized)
        return;

    // Locate the single view entity.
    Entity viewEntity{0, 0};
    const ExtractedView *view = nullptr;
    ConstQuery<ExtractedView> viewQuery(renderWorld);
    for (auto [ve, ev] : viewQuery)
    {
        viewEntity = ve;
        view = ev;
        break;
    }
    if (!view)
        return;

    // Single directional light. Without one the scene is lit by
    // the ambient term only (intensity 0).
    ExtractedLight light{};
    light.intensity = 0.0f;
    ConstQuery<ExtractedLight> lightQuery(renderWorld);
    for (auto [le, el] : lightQuery)
    {
        (void)le;
        light = *el;
        break;
    }

    const ViewBinnedPhases *binned = renderWorld.getComponent<ViewBinnedPhases>(viewEntity);
    const ViewSortedPhases *sorted = renderWorld.getComponent<ViewSortedPhases>(viewEntity);
    if (!binned && !sorted)
        return;

    IRHICommandList *cmdList = m_device->createCommandList();

    // Sky gradient pass: right after the main loop's clear,
    // before the opaque phase. No SkySettings in the main world means no
    // ExtractedSky and the plain clear color shows through.
    if (m_sky_ready)
    {
        ConstQuery<ExtractedSky> skyQuery(renderWorld);
        for (auto [skyEntity, sky] : skyQuery)
        {
            (void)skyEntity;
            if (sky->enabled)
                drawSky(*view, *sky, cmdList);
            break;
        }
    }

    if (binned)
    {
        for (const PhaseBin &bin : binned->opaque.getBins())
        {
            for (const PhaseItem &item : bin.items)
            {
                drawItem(renderWorld, *view, light, item.render_entity, cmdList, prepare);
            }
        }
        for (const PhaseBin &bin : binned->alpha_mask.getBins())
        {
            for (const PhaseItem &item : bin.items)
            {
                drawItem(renderWorld, *view, light, item.render_entity, cmdList, prepare);
            }
        }
    }
    if (sorted)
    {
        for (const PhaseItem &item : sorted->transparent.getItems())
        {
            drawItem(renderWorld, *view, light, item.render_entity, cmdList, prepare);
        }
        for (const PhaseItem &item : sorted->ui.getItems())
        {
            drawItem(renderWorld, *view, light, item.render_entity, cmdList, prepare);
        }
    }

    m_device->submit(cmdList);
}

void RenderExecuteSystem::endFrame()
{
    if (!m_initialized || !m_device)
        return;
    m_device->signalFrame();
    m_device->flushPendingDeletes();
}

} // namespace Entelechy
