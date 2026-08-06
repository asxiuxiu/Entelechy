#pragma once
#include "core/foundation_types.h"

namespace Entelechy
{

// Editable directional-light state mirrored from the main-world
// DirectionalLight component by the main loop. Kept as plain floats so
// ImGuiLib stays a pure UI layer (no render_system/EcsLib dependency).
struct DirectionalLightParams
{
    f32 direction[3] = {0.0f, -1.0f, 0.0f}; // direction the light travels
    f32 color[3] = {1.0f, 1.0f, 1.0f};
    f32 intensity = 1.0f;
    f32 ambient = 0.03f;
};

// Editable sky-gradient state mirrored from the main-world SkySettings
// component by the main loop (Phase 5c, D6). Plain floats keep ImGuiLib a
// pure UI layer, same as DirectionalLightParams.
struct SkyParams
{
    f32 horizonColor[3] = {0.55f, 0.65f, 0.75f};
    f32 zenithColor[3] = {0.10f, 0.23f, 0.55f};
    bool enabled = true;
};

// Request emitted by the debug panel when the user chooses a new resolution.
struct WindowSizeRequest
{
    int width = 0;
    int height = 0;
    bool requested = false;
};

// Read-only snapshot for the render stats panel (Phase 5c, D7). Plain
// scalars keep ImGuiLib free of render_system/RHI types.
struct RenderStatsParams
{
    f32 fps = 0.0f;
    u32 drawCalls = 0;
    u32 visible = 0;
    u32 culled = 0;
    u32 total = 0;
    u32 psoCacheSize = 0;
    u64 trackedMemoryBytes = 0; // RHI-tracked resident GPU memory (always valid)
    u64 gpuTotalBytes = 0;      // vendor extension (NVX/ATI); 0 = unsupported
    u64 gpuAvailableBytes = 0;  // vendor extension (NVX/ATI); 0 = unsupported
};

// Build the global DockSpace over the main viewport.
// Call once per frame, before any other panel Begin/End blocks.
void buildDockSpace();

// Debug panel: FPS, frame time, clear color picker, directional light
// controls, sky gradient controls, resolution settings. Parameters are
// in/out so ImGui sliders edit engine state directly. light and sky may be
// null (the corresponding section is not rendered then).
// outRequest is populated when the user clicks a resolution preset.
void buildDebugPanel(f32 dt, f32 fps, f32 clearColor[4], DirectionalLightParams *light, SkyParams *sky,
                     int windowWidth, int windowHeight, WindowSizeRequest &outRequest);

// Log panel: renders the async logger's history ring buffer with
// level filtering, color coding, and auto-scroll.
void buildLogPanel();

// Render stats panel: FPS, draw calls, culling counters, PSO cache size and
// GPU memory from the previous frame's render pipeline run
// (RenderFrameRunner::stats() + the main loop's FPS counter).
void buildRenderStatsPanel(const RenderStatsParams &stats);

} // namespace Entelechy
