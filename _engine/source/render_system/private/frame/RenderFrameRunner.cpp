#include "render_system/frame/RenderFrameRunner.h"
#include "render_system/components/RenderCamera.h"
#include "render_system/components/RenderComponents.h"
#include "render_system/culling/ViewVisibleList.h"
#include "render/rhi/rhi_device.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"
#include "ecs/component/component_array.h"
#include "log/core/log_macros.h"

namespace Entelechy
{

bool RenderFrameRunner::init(IWindow *window)
{
    if (m_initialized)
        return true;

    m_extract_camera = ExtractCameraSystem(window);
    m_render_world.extractSchedule().registerSystem(&m_extract_camera);
    m_render_world.extractSchedule().registerSystem(&m_extract_light);
    m_render_world.extractSchedule().registerSystem(&m_extract_sky);
    m_render_world.extractSchedule().registerSystem(&m_extract_renderables);

    if (!m_execute.init())
    {
        LOG_ERROR(LogCategories::kEngine, "RenderFrameRunner: failed to init RenderExecuteSystem");
        return false;
    }

    if (!m_prepare.init(m_execute.device(), m_execute.shaderCache()))
    {
        LOG_ERROR(LogCategories::kEngine, "RenderFrameRunner: failed to init PrepareAssetsSystem");
        return false;
    }

    m_initialized = true;
    return true;
}

void RenderFrameRunner::runFrame(const World &mainWorld, f32 dt)
{
    if (!m_initialized)
        return;

    m_stats = FrameStats{};

    m_render_world.extract(mainWorld, dt);

    World &renderWorld = m_render_world.world();
    m_prepare.run(renderWorld);
    m_cull.run(renderWorld);
    m_queue.run(renderWorld);
    m_execute.run(renderWorld, m_prepare);

    // -- Frame statistics ------------------------------------------------
    const ComponentArray<RenderTransform> *transforms = renderWorld.getComponentArray<RenderTransform>();
    m_stats.total_renderables = transforms ? static_cast<u32>(transforms->count()) : 0;

    ConstQuery<ExtractedView> viewQuery(renderWorld);
    for (auto [viewEntity, view] : viewQuery)
    {
        (void)view;
        const ViewVisibleList *visibleList = renderWorld.getComponent<ViewVisibleList>(viewEntity);
        m_stats.visible = visibleList ? static_cast<u32>(visibleList->entities.size()) : 0;
        break;
    }
    m_stats.culled = m_stats.total_renderables - m_stats.visible;
    m_stats.draw_calls = m_execute.stats().draw_calls;

    // Phase 5c (D7): PSO cache + GPU memory counters for the stats panel.
    // queryMemoryInfo() returns zeros when neither vendor extension exists.
    if (IRHIDevice *device = m_execute.device())
    {
        m_stats.pso_cache_size = static_cast<u32>(m_execute.psoCacheSize());
        m_stats.tracked_memory_bytes = device->getTrackedMemoryUsage();
        const RHIMemoryInfo memInfo = device->queryMemoryInfo();
        m_stats.gpu_total_bytes = memInfo.totalBytes;
        m_stats.gpu_available_bytes = memInfo.availableBytes;
    }
}

void RenderFrameRunner::endFrame()
{
    if (!m_initialized)
        return;
    m_execute.endFrame();
}

void RenderFrameRunner::shutdown()
{
    if (!m_initialized)
        return;
    // Prepare releases GPU resources (via deferred delete) before the
    // execute stage tears down the device.
    m_prepare.shutdown();
    m_execute.shutdown();
    m_render_world.clear();
    m_initialized = false;
}

} // namespace Entelechy
