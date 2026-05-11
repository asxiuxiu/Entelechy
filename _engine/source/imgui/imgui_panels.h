#pragma once
#include "foundation_types.h"

namespace Entelechy {

// Forward declarations
class World;
class Scheduler;

// Request emitted by the debug panel when the user chooses a new resolution.
struct WindowSizeRequest {
    int width = 0;
    int height = 0;
    bool requested = false;
};

// Build the global DockSpace over the main viewport.
// Call once per frame, before any other panel Begin/End blocks.
void buildDockSpace();

// Debug panel: FPS, frame time, clear color picker, resolution settings.
// Parameters are in/out so ImGui sliders edit engine state directly.
// outRequest is populated when the user clicks a resolution preset.
void buildDebugPanel(f32 dt, f32 fps, f32 clearColor[4],
                     int windowWidth, int windowHeight,
                     WindowSizeRequest& outRequest);

// Log panel: renders the async logger's history ring buffer with
// level filtering, color coding, and auto-scroll.
void buildLogPanel();

// ECS Inspector: two-column layout (Entity list + Component detail).
// autoRun is an in/out parameter controlling Scheduler tick.
void buildECSInspector(World& world, Scheduler& scheduler, f32 dt, bool& autoRun);

} // namespace Entelechy
