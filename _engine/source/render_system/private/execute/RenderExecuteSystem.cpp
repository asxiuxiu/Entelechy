#include "render_system/execute/RenderExecuteSystem.h"
#include "render_system/prepare/PrepareAssetsSystem.h"
#include "render_system/components/RenderCamera.h"
#include "render_system/components/RenderComponents.h"
#include "render_system/phase/RenderResources.h"
#include "render/rhi/gl_rhi_device.h"
#include "render/material/shader_cache.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"
#include "log/core/log_macros.h"

namespace Entelechy
{

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

    m_initialized = true;
    LOG_INFO(LogCategories::kEngine, "RenderExecuteSystem initialized (RHI + ShaderCache)");
    return true;
}

void RenderExecuteSystem::shutdown()
{
    if (!m_initialized)
        return;

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

void RenderExecuteSystem::drawItem(World &renderWorld, const ExtractedView &view, Entity renderEntity,
                                   IRHICommandList *cmdList, PrepareAssetsSystem &prepare)
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

    prepared->material.setMat4("uMVP"_sid, view.proj_matrix * view.view_matrix * transform->world_matrix);
    prepared->material.setMat4("uModel"_sid, transform->world_matrix);
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

    const ViewBinnedPhases *binned = renderWorld.getComponent<ViewBinnedPhases>(viewEntity);
    const ViewSortedPhases *sorted = renderWorld.getComponent<ViewSortedPhases>(viewEntity);
    if (!binned && !sorted)
        return;

    IRHICommandList *cmdList = m_device->createCommandList();

    if (binned)
    {
        for (const PhaseBin &bin : binned->opaque.getBins())
        {
            for (const PhaseItem &item : bin.items)
            {
                drawItem(renderWorld, *view, item.render_entity, cmdList, prepare);
            }
        }
        for (const PhaseBin &bin : binned->alpha_mask.getBins())
        {
            for (const PhaseItem &item : bin.items)
            {
                drawItem(renderWorld, *view, item.render_entity, cmdList, prepare);
            }
        }
    }
    if (sorted)
    {
        for (const PhaseItem &item : sorted->transparent.getItems())
        {
            drawItem(renderWorld, *view, item.render_entity, cmdList, prepare);
        }
        for (const PhaseItem &item : sorted->ui.getItems())
        {
            drawItem(renderWorld, *view, item.render_entity, cmdList, prepare);
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
