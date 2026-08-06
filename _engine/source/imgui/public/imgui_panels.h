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

// Request emitted by the debug panel when the user chooses a new resolution.
struct WindowSizeRequest
{
    int width = 0;
    int height = 0;
    bool requested = false;
};

// Build the global DockSpace over the main viewport.
// Call once per frame, before any other panel Begin/End blocks.
void buildDockSpace();

// Debug panel: FPS, frame time, clear color picker, directional light
// controls, resolution settings. Parameters are in/out so ImGui sliders edit
// engine state directly. light may be null (no light section rendered then).
// outRequest is populated when the user clicks a resolution preset.
void buildDebugPanel(f32 dt, f32 fps, f32 clearColor[4], DirectionalLightParams *light, int windowWidth,
                     int windowHeight, WindowSizeRequest &outRequest);

// Log panel: renders the async logger's history ring buffer with
// level filtering, color coding, and auto-scroll.
void buildLogPanel();

// Render stats panel: draw calls and culling counters from the previous
// frame's render pipeline run (RenderFrameRunner::stats()).
void buildRenderStatsPanel(u32 drawCalls, u32 visible, u32 culled, u32 total);

} // namespace Entelechy
