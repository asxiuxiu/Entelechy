#include "runtime/game_plugin.h"
#include "runtime/render_assets.h"
#include "runtime/scene_loader.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"
#include "ecs/type/type_registry.h"
#include "core/math/aabb.h"
#include "core/string/string_intern_pool.h"
#include "render_system/components/Camera.h"

namespace game
{

namespace
{

// AABB is not registered as an ECS component by the engine (tracked in
// TODO.md as a future WorldAABB wrapper component). Register it here so the
// cooked scene entities can carry world-space bounds for frustum culling.
void registerAabbComponent()
{
    using namespace Entelechy;
    TypeRegistry &registry = TypeRegistry::instance();
    if (registry.getTypeID<AABB>() != INVALID_COMPONENT_TYPE_ID)
        return;
    ComponentTypeID id = registry.getOrAllocateTypeID<AABB>();
    registry.registerComponent(id, 1ull << id, makeComponentDesc<AABB>("AABB"_sid, {}));
}

} // namespace

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

    registerAabbComponent();
    initRenderAssets();

    // -- Free-fly camera ---------------------------------------------------
    // Inside the Sponza atrium (scene spans x[-16, 20]), near the west end
    // looking down the +X axis; FlyCameraSystem's initial yaw matches.
    auto camera = world.spawn();
    world.addComponent<Transform>(camera, Transform{{-13.0f, 2.5f, 2.0f}});
    world.addComponent<GlobalTransform>(camera, GlobalTransform{});
    world.addComponent<Camera>(camera, Camera{1.0472f, 0.1f, 200.0f, false, 10.0f});
    world.addComponent<FlyCameraTag>(camera, FlyCameraTag{});

    // -- Cooked Sponza scene (Phase 3c) -------------------------------------
    // Spawns one entity per scene.json entry (405 for NewSponza); meshes
    // stream in asynchronously and draw as pink fallback cubes until ready.
    spawnCookedScene(world, renderAssets(), "sponza/cooked/scene.json");
}

} // namespace game
