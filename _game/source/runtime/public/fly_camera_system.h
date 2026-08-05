#pragma once
#include "ecs/world/scheduler.h"

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
// The InputQueue singleton only carries discrete events, so the system keeps
// held-key and drag state between frames. There is no mouse capture yet
// (right-drag instead of FPS-style capture — tracked in TODO.md).
class FlyCameraSystem : public Entelechy::System
{
public:
    void tick(Entelechy::World &world, Entelechy::FrameArena &arena, f32 dt) override;

private:
    static constexpr int MAX_TRACKED_KEYS = 512; // GLFW key codes fit in [0, 348]

    bool m_key_held[MAX_TRACKED_KEYS] = {};
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
