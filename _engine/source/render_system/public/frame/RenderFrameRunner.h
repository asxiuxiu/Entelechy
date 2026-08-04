#pragma once
#include "core/foundation_types.h"
#include "render_system/render_world/RenderWorld.h"
#include "render_system/extract/ExtractCameraSystem.h"
#include "render_system/extract/ExtractRenderablesSystem.h"
#include "render_system/culling/FrustumCullSystem.h"
#include "render_system/queue/QueueDrawsSystem.h"
#include "render_system/execute/RenderExecuteSystem.h"

namespace Entelechy
{

class IWindow;
class World;

// FrameStats — per-frame counters across the whole render pipeline.
struct FrameStats
{
    u32 total_renderables = 0;
    u32 visible = 0;
    u32 culled = 0;
    u32 draw_calls = 0;
};

// RenderFrameRunner — production frame driver that chains the Phase-1 render
// pipeline: Extract -> Cull -> Queue -> Execute. Owns the RenderWorld and all
// pipeline systems; the main loop only calls init/runFrame/endFrame/shutdown.
class RenderFrameRunner
{
public:
    RenderFrameRunner() = default;

    RenderFrameRunner(const RenderFrameRunner &) = delete;
    RenderFrameRunner &operator=(const RenderFrameRunner &) = delete;

    // Registers the extract systems on the RenderWorld and initializes the
    // execute stage (RHI device + shader cache). Window is used for aspect
    // ratio and viewport extraction.
    bool init(IWindow *window);

    // Runs Extract -> Cull -> Queue -> Execute for one frame.
    void runFrame(const World &mainWorld, f32 dt);

    // Frame fence + deferred-delete flush. Call once per frame after present.
    void endFrame();

    void shutdown();

    // GPU resource registration entry (Phase 1: manual registration by the
    // caller; replaced by the Prepare stage in Phase 2).
    RenderExecuteSystem &execute()
    {
        return m_execute;
    }

    [[nodiscard]] const FrameStats &stats() const
    {
        return m_stats;
    }

private:
    RenderWorld m_render_world;
    ExtractCameraSystem m_extract_camera{nullptr}; // window bound in init()
    ExtractRenderablesSystem m_extract_renderables{m_render_world.sync()};
    FrustumCullSystem m_cull;
    QueueDrawsSystem m_queue;
    RenderExecuteSystem m_execute;
    FrameStats m_stats;
    bool m_initialized = false;
};

} // namespace Entelechy
