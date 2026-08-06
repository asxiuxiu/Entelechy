#include "render_system/execute/render_execute_system.h"
#include "render_system/prepare/prepare_assets_system.h"
#include "render_system/components/render_camera.h"
#include "render_system/components/render_components.h"
#include "render_system/components/render_light.h"
#include "render_system/components/render_sky.h"
#include "core/math/mat3.h"
#include "render_system/phase/render_resources.h"
#include "render/rhi/gl_rhi_device.h"
#include "render/material/shader_cache.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"
#include "log/core/log_macros.h"

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

bool RenderExecuteSystem::init()
{
    if (m_initialized)
        return true;

    m_device = std::make_unique<GLRHIDevice>();
    if (!m_device->initialize())
    {
        LOG_ERROR(LogCategories::kEngine, "RenderExecuteSystem: failed to initialize GLRHIDevice");
        return false;
    }

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
    if (m_device)
    {
        m_device->shutdown();
        m_device.reset();
    }
    m_initialized = false;
}

IRHIDevice *RenderExecuteSystem::device()
{
    return m_device.get();
}

ShaderCache *RenderExecuteSystem::shaderCache()
{
    return m_shader_cache.get();
}

usize RenderExecuteSystem::psoCacheSize() const
{
    return m_device ? m_device->getPSOManager().getCacheSize() : 0;
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

    MaterialParamDesc params[] = {
        {"uInvViewProj", MaterialParamType::Mat4},
        {"uViewPos", MaterialParamType::Vec3},
        {"uHorizonColor", MaterialParamType::Vec3},
        {"uZenithColor", MaterialParamType::Vec3},
    };
    PipelineStateDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangles;
    pipelineDesc.rasterizerState.cullMode = CullMode::None;
    // The triangle sits at NDC z = 1 against the freshly cleared depth
    // buffer, so LessEqual passes everywhere the scene has not drawn yet;
    // depth write stays off and the opaque phase overwrites the sky normally.
    pipelineDesc.depthStencilState.depthTest = true;
    pipelineDesc.depthStencilState.depthWrite = false;
    pipelineDesc.depthStencilState.depthFunc = CompareFunc::LessEqual;

    if (!m_sky_material.init(m_device.get(), m_shader_cache.get(), s_skyVertexShader, s_skyFragmentShader, params, 4,
                             pipelineDesc))
        return false;

    m_sky_material.setVec3("uHorizonColor"_sid, Vec3{0.55f, 0.65f, 0.75f});
    m_sky_material.setVec3("uZenithColor"_sid, Vec3{0.10f, 0.23f, 0.55f});
    return true;
}

void RenderExecuteSystem::drawSky(const ExtractedView &view, const ExtractedSky &sky, IRHICommandList *cmdList)
{
    m_sky_material.setMat4("uInvViewProj"_sid, (view.proj_matrix * view.view_matrix).inverse());
    m_sky_material.setVec3("uViewPos"_sid, view.view_pos);
    m_sky_material.setVec3("uHorizonColor"_sid, sky.horizon_color);
    m_sky_material.setVec3("uZenithColor"_sid, sky.zenith_color);
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

    // Per-draw object uniforms: uMVP + world matrix + its
    // inverse-transpose 3x3 for normals. Lighting uniforms are view-level but
    // go through the same per-draw material mechanism until uniform data is
    // frequency-layered (TODO.md Render/UniformBinding).
    prepared->material.setMat4("uMVP"_sid, view.proj_matrix * view.view_matrix * transform->world_matrix);
    prepared->material.setMat4("uModel"_sid, transform->world_matrix);
    prepared->material.setMat3("uNormalMatrix"_sid, Mat3::normalMatrix(transform->world_matrix));
    prepared->material.setVec3("uViewPos"_sid, view.view_pos);
    prepared->material.setVec3("uLightDir"_sid, light.direction);
    prepared->material.setVec3("uLightColor"_sid, light.color);
    prepared->material.setFloat("uLightIntensity"_sid, light.intensity);
    prepared->material.setFloat("uAmbient"_sid, light.ambient);
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
