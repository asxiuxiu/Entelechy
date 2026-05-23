#pragma once
#include "core/plugin.h"
#include "core/app.h"
#include "scheduler.h"
#include "movement_system.h"
#include "math/transform_propagation_system.h"
#include "core/event_cleanup_system.h"
#include "core/events.h"
#include "core/event_lifetime.h"
#include "math/transform_component.h"
#include "types.h"

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
    } m_colorChange;

    Entelechy::TransformPropagationSystem m_transformSystem;
    Entelechy::EventCleanupSystem m_eventCleanup;
};

} // namespace game
