#include "runtime/game_plugin.h"
#include "runtime/render_assets.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"
#include "ecs/type/type_registry.h"
#include "core/string/string_intern_pool.h"
#include "render_system/components/camera.h"
#include "render_system/components/directional_light.h"
#include "render_system/components/sky_settings.h"

namespace game
{

void GamePlugin::build(Entelechy::App &app)
{
    using namespace Entelechy;

    // MovementSystem
    app.scheduler().registerSystem(
        {.name = StringInternPool::instance().intern("MovementSystem"),
         .system = &m_movement,
         .phase = static_cast<u8>(DefaultPhase::Update),
         .reads = {TypeRegistry::instance().getTypeID<Position>(), TypeRegistry::instance().getTypeID<Velocity>()},
         .writes = {TypeRegistry::instance().getTypeID<Position>()}});

    // FlyCameraSystem
    app.scheduler().registerSystem({.name = StringInternPool::instance().intern("FlyCameraSystem"),
                                    .system = &m_fly_camera,
                                    .phase = static_cast<u8>(DefaultPhase::Update),
                                    .writes = {TypeRegistry::instance().getTypeID<Transform>()}});

    // MaterialTextureBackfillSystem: issues texture loads
    // for async-arrived scene materials via the engine SceneLoader.
    // Touches no ECS components.
    app.scheduler().registerSystem(
        {.name = StringInternPool::instance().intern("MaterialTextureBackfillSystem"),
         .system = &m_material_backfill,
         .phase = static_cast<u8>(DefaultPhase::Update)});

    // TransformPropagationSystem
    app.scheduler().registerSystem(
        {.name = StringInternPool::instance().intern("TransformPropagationSystem"),
         .system = &m_transform_system,
         .phase = static_cast<u8>(DefaultPhase::PostUpdate),
         .reads = {TypeRegistry::instance().getTypeID<Transform>(), TypeRegistry::instance().getTypeID<ChildOf>()},
         .writes = {TypeRegistry::instance().getTypeID<GlobalTransform>()}});

    // EventCleanupSystem
    app.scheduler().registerSystem({.name = StringInternPool::instance().intern("EventCleanupSystem"),
                                    .system = &m_event_cleanup,
                                    .phase = static_cast<u8>(DefaultPhase::Last),
                                    .reads = {TypeRegistry::instance().getTypeID<EventLifetime>()}});
}

void GamePlugin::setup(Entelechy::App &app)
{
    using namespace Entelechy;
    World &world = app.world();

    initRenderAssets();

    // -- Free-fly camera ---------------------------------------------------
    // Inside the Sponza atrium (scene spans x[-16, 20]), near the west end
    // looking down the +X axis; FlyCameraSystem's initial yaw matches.
    auto camera = world.spawn();
    world.addComponent<Transform>(camera, Transform{{-13.0f, 2.5f, 2.0f}});
    world.addComponent<GlobalTransform>(camera, GlobalTransform{});
    world.addComponent<Camera>(camera, Camera{1.0472f, 0.1f, 200.0f, false, 10.0f});
    world.addComponent<FlyCameraTag>(camera, FlyCameraTag{});

    // -- Sun -----------------------------------------------------
    // Single directional light roughly matching the official Sponza renders:
    // warm daylight slanting into the atrium from one side. Tunable at
    // runtime through the ImGui debug panel.
    auto sun = world.spawn();
    world.addComponent<DirectionalLight>(
        sun, DirectionalLight{{0.45f, -0.85f, -0.25f}, {1.0f, 0.956f, 0.839f}, 3.0f, 0.15f});

    // -- Sky gradient --------------------------------------------
    // Daylight gradient: hazy bright horizon to a deeper blue zenith. Tunable
    // at runtime through the ImGui debug panel.
    auto sky = world.spawn();
    world.addComponent<SkySettings>(sky, SkySettings{});

    // -- Cooked Sponza scene --------------------------------
    // Spawns one entity per scene.json entry (405 for NewSponza); meshes
    // stream in asynchronously and draw as pink fallback cubes until ready.
    // The engine SceneLoader owns manifest parsing and material assembly;
    // the game side only passes the scene path.
    renderAssets().scene_loader.spawnCookedScene(world, "sponza/cooked/scene.json");
}

} // namespace game
