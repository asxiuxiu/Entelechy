#pragma once
#include "ecs/world/plugin.h"
#include "ecs/world/app.h"
#include "ecs/world/scheduler.h"
#include "motor/movement_system.h"
#include "ecs/system/transform_propagation_system.h"
#include "ecs/event/event_cleanup_system.h"
#include "ecs/event/events.h"
#include "ecs/event/event_lifetime.h"
#include "ecs/component/transform_component.h"
#include "ecs/type/types.h"

namespace game {

// ------------------------------------------------------------------
// GamePlugin -- demo game logic plugin
// ------------------------------------------------------------------
// Registers demo systems (movement, rotation, wobble, color change,
// transform propagation, event cleanup) and spawns the demo cube
// hierarchy.
// ------------------------------------------------------------------
class GamePlugin : public Entelechy::IPlugin {
public:
    const char* name() const override { return "GamePlugin"; }
    void build(Entelechy::App& app) override;
    void setup(Entelechy::App& app) override;

private:
    Entelechy::MovementSystem m_movement;

    struct RotationSystem : Entelechy::System {
        void tick(Entelechy::World& w, Entelechy::FrameArena&, f32 dt) override;
    } m_rotation;

    struct WobbleSystem : Entelechy::System {
        void tick(Entelechy::World& w, Entelechy::FrameArena&, f32 dt) override;
    } m_wobble;

    struct ColorChangeSystem : Entelechy::System {
        void tick(Entelechy::World& w, Entelechy::FrameArena&, f32) override;
    } m_color_change;

    Entelechy::TransformPropagationSystem m_transform_system;
    Entelechy::EventCleanupSystem m_event_cleanup;
};

} // namespace game
