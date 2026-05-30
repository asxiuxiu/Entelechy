#include "game_plugin.h"
#include "world.h"
#include "query.h"
#include "type_registry.h"
#include "math/hierarchy.h"
#include <cstdlib>

namespace game {

void GamePlugin::build(Entelechy::App& app) {
    using namespace Entelechy;

    // MovementSystem
    app.scheduler().registerSystem({
        .name = "MovementSystem",
        .system = &m_movement,
        .phase = static_cast<u8>(DefaultPhase::Update),
        .reads = { TypeRegistry::instance().getTypeID<Position>(),
                   TypeRegistry::instance().getTypeID<Velocity>() },
        .writes = { TypeRegistry::instance().getTypeID<Position>() }
    });

    // RotationSystem
    app.scheduler().registerSystem({
        .name = "RotationSystem",
        .system = &m_rotation,
        .phase = static_cast<u8>(DefaultPhase::Update),
        .reads = { TypeRegistry::instance().getTypeID<Transform>() },
        .writes = { TypeRegistry::instance().getTypeID<Transform>() }
    });

    // WobbleSystem (deliberately conflicts with RotationSystem to verify ambiguity detection)
    app.scheduler().registerSystem({
        .name = "WobbleSystem",
        .system = &m_wobble,
        .phase = static_cast<u8>(DefaultPhase::Update),
        .writes = { TypeRegistry::instance().getTypeID<Transform>() }
    });

    // TransformPropagationSystem
    app.scheduler().registerSystem({
        .name = "TransformPropagationSystem",
        .system = &m_transform_system,
        .phase = static_cast<u8>(DefaultPhase::PostUpdate),
        .reads = { TypeRegistry::instance().getTypeID<Transform>(),
                   TypeRegistry::instance().getTypeID<ChildOf>() },
        .writes = { TypeRegistry::instance().getTypeID<GlobalTransform>() }
    });

    // ColorChangeSystem
    app.scheduler().registerSystem({
        .name = "ColorChangeSystem",
        .system = &m_color_change,
        .phase = static_cast<u8>(DefaultPhase::Update),
        .reads = { TypeRegistry::instance().getTypeID<KeyboardEvent>() },
        .writes = { TypeRegistry::instance().getTypeID<Color>() }
    });

    // EventCleanupSystem
    app.scheduler().registerSystem({
        .name = "EventCleanupSystem",
        .system = &m_event_cleanup,
        .phase = static_cast<u8>(DefaultPhase::Last),
        .reads = { TypeRegistry::instance().getTypeID<EventLifetime>() }
    });
}

void GamePlugin::setup(Entelechy::App& app) {
    using namespace Entelechy;
    World& world = app.world();

    // Demo cubes with hierarchy
    auto parent = world.spawn();
    world.addComponent<Transform>(parent, Transform{});
    world.addComponent<GlobalTransform>(parent, GlobalTransform{});
    world.addComponent<Color>(parent, {1.0f, 0.2f, 0.2f});

    auto child1 = world.spawn();
    world.addComponent<Transform>(child1, Transform{{1.5f, 0.0f, 0.0f}});
    world.addComponent<GlobalTransform>(child1, GlobalTransform{});
    world.addComponent<Color>(child1, {0.2f, 1.0f, 0.2f});
    world.setParent(child1, parent);

    auto child2 = world.spawn();
    world.addComponent<Transform>(child2, Transform{{-1.5f, 0.0f, 0.0f}});
    world.addComponent<GlobalTransform>(child2, GlobalTransform{});
    world.addComponent<Color>(child2, {0.2f, 0.2f, 1.0f});
    world.setParent(child2, parent);

    auto grandchild = world.spawn();
    world.addComponent<Transform>(grandchild, Transform{{0.0f, 1.0f, 0.0f}});
    world.addComponent<GlobalTransform>(grandchild, GlobalTransform{});
    world.addComponent<Color>(grandchild, {1.0f, 1.0f, 0.2f});
    world.setParent(grandchild, child1);
}

// ------------------------------------------------------------------
// System tick implementations
// ------------------------------------------------------------------

void GamePlugin::RotationSystem::tick(Entelechy::World& w, Entelechy::FrameArena&, f32 dt) {
    for (auto [e, trans] : Entelechy::Query<Entelechy::Transform>(w)) {
        if (!trans) continue;
        trans->rotation = trans->rotation * Entelechy::Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, dt);
        trans->dirty = 1;
    }
}

void GamePlugin::WobbleSystem::tick(Entelechy::World& w, Entelechy::FrameArena&, f32 dt) {
    (void)dt;
    for (auto [e, trans] : Entelechy::Query<Entelechy::Transform>(w)) {
        if (!trans) continue;
        trans->translation.y += 0.001f;
        trans->dirty = 1;
    }
}

void GamePlugin::ColorChangeSystem::tick(Entelechy::World& w, Entelechy::FrameArena&, f32) {
    for (auto [evtEntity, evt] : Entelechy::Query<Entelechy::KeyboardEvent>(w)) {
        if (!evt || !evt->pressed) continue;
        if (evt->keyCode == 32) { // GLFW_KEY_SPACE
            for (auto [e, color] : Entelechy::Query<Entelechy::Color>(w)) {
                if (color) {
                    color->r = 0.3f + (rand() % 100) / 100.0f;
                    color->g = 0.3f + (rand() % 100) / 100.0f;
                    color->b = 0.3f + (rand() % 100) / 100.0f;
                }
            }
        }
    }
}

} // namespace game
