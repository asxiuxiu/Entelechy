#include "render_system/execute/render_execute_system.h"
#include "render_system/prepare/prepare_assets_system.h"
#include "render_system/components/render_camera.h"
#include "render_system/components/render_components.h"
#include "render_system/components/render_light.h"
#include "render_system/components/render_sky.h"
#include "render_system/phase/render_resources.h"
#include "render/binding/constant_buffer_ring.h"
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

    // Per-frame constant data ring (UBO on GL, upload-heap CBV on D3D12).
    m_ring = std::make_unique<ConstantBufferRing>();
    if (!m_ring->init(device))
    {
        LOG_ERROR(LogCategories::kEngine, "RenderExecuteSystem: constant buffer ring init failed");
        m_ring.reset();
        return false;
    }

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
    m_ring.reset();
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

    // Sky shader cbuffers (reflection-driven, 6e):
    //   PerFrame(b0, VS):   uInvViewProj (mat4)
    //   PerFramePS(b1, PS): uViewPos / uHorizonColor / uZenithColor (vec4)
    MaterialParamDesc params[] = {
        {"uInvViewProj", MaterialParamType::Mat4},
        {"uViewPos", MaterialParamType::Vec4},
        {"uHorizonColor", MaterialParamType::Vec4},
        {"uZenithColor", MaterialParamType::Vec4},
    };
    constexpr u32 s_skyParamCount = 4;
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

    // Set default sky colors using reflected member names
    m_sky_material.setVec4("uHorizonColor"_sid, Vec4{0.55f, 0.65f, 0.75f, 0.0f});
    m_sky_material.setVec4("uZenithColor"_sid, Vec4{0.10f, 0.23f, 0.55f, 0.0f});
    return true;
}

void RenderExecuteSystem::drawSky(const ExtractedView &view, const ExtractedSky &sky, IRHICommandList *cmdList)
{
    // PerFrame(b0, VS): uInvViewProj; PerFramePS(b1, PS): uViewPos + colors
    Mat4 invVP = (view.proj_matrix * view.view_matrix).inverse();
    m_sky_material.setMat4("uInvViewProj"_sid, invVP);
    m_sky_material.setVec4("uViewPos"_sid, Vec4{view.view_pos.x, view.view_pos.y, view.view_pos.z, 0.0f});
    m_sky_material.setVec4("uHorizonColor"_sid,
                           Vec4{sky.horizon_color.x, sky.horizon_color.y, sky.horizon_color.z, 0.0f});
    m_sky_material.setVec4("uZenithColor"_sid,
                           Vec4{sky.zenith_color.x, sky.zenith_color.y, sky.zenith_color.z, 0.0f});
    m_sky_material.bind(cmdList, m_ring.get());
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

    // Per-draw constants using the reflected member names (6e):
    //   PerFrame(b0, PS): uViewPos / uLightDir(dir.xyz + intensity.w) / uLightColor(color.xyz + ambient.w)
    //   PerDraw(b2, VS):  uMVP / uModel / uNormalMatrix (mat4 carrying a mat3)
    prepared->material.setVec4("uViewPos"_sid, Vec4{view.view_pos.x, view.view_pos.y, view.view_pos.z, 0.0f});
    prepared->material.setVec4("uLightDir"_sid,
                               Vec4{light.direction.x, light.direction.y, light.direction.z, light.intensity});
    prepared->material.setVec4("uLightColor"_sid,
                               Vec4{light.color.x, light.color.y, light.color.z, light.ambient});

    Mat4 mvp = view.proj_matrix * view.view_matrix * transform->world_matrix;
    prepared->material.setMat4("uMVP"_sid, mvp);

    const Mat4 &model = transform->world_matrix;
    prepared->material.setMat4("uModel"_sid, model);

    // Normal matrix as a 4x4 with the mat3 in the upper-left 3x3 (column-
    // major, matching the shader's (float3x3)uNormalMatrix cast).
    Mat3 normalMat = Mat3::normalMatrix(model);
    Mat4 normalMat4{};
    for (int col = 0; col < 3; ++col)
    {
        for (int row = 0; row < 3; ++row)
            normalMat4.m[col * 4 + row] = normalMat.m[col * 3 + row];
    }
    prepared->material.setMat4("uNormalMatrix"_sid, normalMat4);

    prepared->material.bind(cmdList, m_ring.get());

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
