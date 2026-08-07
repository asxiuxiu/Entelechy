#include "runtime/fly_camera_system.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"
#include "ecs/component/transform_component.h"
#include "window/input/input_queue.h"
#include "window/window.h"
#include "core/math/quat.h"
#include "core/string/string_id.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace game
{

using Entelechy::operator""_sid;

REFLECT_COMPONENT(FlyCameraTag)

namespace
{

constexpr f32 MOVE_SPEED = 5.0f;   // units per second
constexpr f32 BOOST_FACTOR = 4.0f; // Shift multiplier
constexpr f32 LOOK_SENSITIVITY = 0.003f;
constexpr f32 PITCH_LIMIT = 1.5f; // radians

} // namespace

void FlyCameraSystem::tick(Entelechy::World &world, Entelechy::FrameArena &, f32 dt)
{
    using namespace Entelechy;

    // -- Focus gate: drop all transient input state when the window is not
    // focused. Events arriving while unfocused (or lost during a focus
    // switch) must not leave stale "held"/"dragging" state behind.
    const bool focused = m_window && m_window->hasFocus();
    if (!focused)
    {
        m_looking = false;
    }

    // -- Consume discrete input events to maintain drag state --------------
    // (movement keys are polled below, not derived from these events)
    for (const RawInputEvent &e : InputQueue::instance().events())
    {
        switch (e.type)
        {
        case RawInputEvent::MouseButtonPress:
            if (focused && e.mouseButton == GLFW_MOUSE_BUTTON_RIGHT)
            {
                // Button events carry no cursor position; rebase on the first
                // MouseMove so the drag starts without a jump.
                m_looking = true;
                m_look_rebase = true;
            }
            break;
        case RawInputEvent::MouseButtonRelease:
            if (e.mouseButton == GLFW_MOUSE_BUTTON_RIGHT)
                m_looking = false;
            break;
        case RawInputEvent::MouseMove:
            if (m_looking)
            {
                if (!m_look_rebase)
                {
                    m_yaw -= (e.mx - m_last_mx) * LOOK_SENSITIVITY;
                    m_pitch -= (e.my - m_last_my) * LOOK_SENSITIVITY;
                    m_pitch = std::clamp(m_pitch, -PITCH_LIMIT, PITCH_LIMIT);
                }
                m_look_rebase = false;
                m_last_mx = e.mx;
                m_last_my = e.my;
            }
            break;
        default:
            break;
        }
    }

    // -- Apply orientation and movement to the camera entity ---------------
    for (auto [entity, tag, transform] : Query<FlyCameraTag, Transform>(world))
    {
        (void)entity;
        (void)tag;
        if (!transform)
            continue;

        Quat rotation =
            Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, m_yaw) * Quat::fromAxisAngle({1.0f, 0.0f, 0.0f}, m_pitch);
        transform->rotation = rotation;

        Vec3 forward = rotate(rotation, Vec3{0.0f, 0.0f, -1.0f});
        Vec3 right = rotate(rotation, Vec3{1.0f, 0.0f, 0.0f});

        // Held-key state is polled from the OS via the window (immune to
        // lost key events — see class comment).
        Vec3 move{0.0f, 0.0f, 0.0f};
        if (focused)
        {
            if (m_window->isKeyDown(GLFW_KEY_W))
                move += forward;
            if (m_window->isKeyDown(GLFW_KEY_S))
                move -= forward;
            if (m_window->isKeyDown(GLFW_KEY_D))
                move += right;
            if (m_window->isKeyDown(GLFW_KEY_A))
                move -= right;
            if (m_window->isKeyDown(GLFW_KEY_E))
                move += Vec3{0.0f, 1.0f, 0.0f};
            if (m_window->isKeyDown(GLFW_KEY_Q))
                move -= Vec3{0.0f, 1.0f, 0.0f};
        }

        f32 speed = MOVE_SPEED;
        if (focused && (m_window->isKeyDown(GLFW_KEY_LEFT_SHIFT) || m_window->isKeyDown(GLFW_KEY_RIGHT_SHIFT)))
            speed *= BOOST_FACTOR;

        if (move.lengthSq() > 0.0f)
            transform->translation += move.normalized() * (speed * dt);
        transform->dirty = 1;
    }
}

} // namespace game
