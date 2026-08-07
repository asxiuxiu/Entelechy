#pragma once
#include "ecs/world/scheduler.h"

namespace Entelechy
{
class IWindow;
}

namespace game
{

// FlyCameraTag — marker component for the free-fly camera entity.
struct FlyCameraTag
{
};

// FlyCameraSystem — free-fly camera controller.
// WASD: move in the view plane; Q/E: down/up; hold Shift: 4x speed;
// hold the right mouse button and drag: yaw/pitch look.
//
// Held-key state is POLLED each frame via IWindow::isKeyDown() rather than
// accumulated from press/release events: event-derived state desyncs
// permanently when an event is lost (focus switch mid-press, IME candidate
// window, global-hotkey tools swallowing keys), which showed up as WASD
// intermittently "dying" on some machines. Mouse-look stays event-driven
// (needs deltas), but all state resets on focus loss.
// There is no mouse capture yet (right-drag instead of FPS-style capture —
// tracked in TODO.md).
class FlyCameraSystem : public Entelechy::System
{
public:
    void tick(Entelechy::World &world, Entelechy::FrameArena &arena, f32 dt) override;

    // The window polled for held-key state. Injected by main before the
    // first update; input is ignored while unset.
    void setWindow(Entelechy::IWindow *window)
    {
        m_window = window;
    }

private:
    Entelechy::IWindow *m_window = nullptr;
    bool m_looking = false;
    bool m_look_rebase = false; // skip the delta on the first MouseMove of a drag
    f32 m_last_mx = 0.0f;
    f32 m_last_my = 0.0f;
    // Initial orientation matches the camera spawn in GamePlugin::setup():
    // inside the Sponza atrium at (-13, 2.5, 2) looking down the +X axis.
    f32 m_yaw = -1.5708f; // -pi/2
    f32 m_pitch = 0.0f;
};

} // namespace game
