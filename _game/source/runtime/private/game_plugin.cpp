#include "runtime/game_plugin.h"
#include "runtime/render_assets.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"
#include "ecs/type/type_registry.h"
#include "core/math/aabb.h"
#include "core/string/string_intern_pool.h"
#include "render_system/components/Camera.h"
#include "render_system/components/MeshAssetRef.h"
#include "render_system/components/MaterialAssetRef.h"

namespace game
{

namespace
{

// AABB is not registered as an ECS component by the engine (tracked in
// TODO.md as a future WorldAABB wrapper component). Register it here so the
// cube grid can carry world-space bounds for frustum culling.
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

    // -- Free-fly camera ---------------------------------------------------
    auto camera = world.spawn();
    world.addComponent<Transform>(camera, Transform{{0.0f, 3.0f, 10.0f}});
    world.addComponent<GlobalTransform>(camera, GlobalTransform{});
    world.addComponent<Camera>(camera, Camera{1.0472f, 0.1f, 200.0f, false, 10.0f});
    world.addComponent<FlyCameraTag>(camera, FlyCameraTag{});

    // -- Static cube grid --------------------------------------------------
    // 6x6 grid centered on the origin, spacing 2.0; every other row stacks a
    // second cube on top of the first column cube.
    initRenderAssets();
    const RenderAssets &assets = renderAssets();

    constexpr int GRID_SIZE = 6;
    constexpr f32 SPACING = 2.0f;
    constexpr f32 ORIGIN_OFFSET = -0.5f * SPACING * static_cast<f32>(GRID_SIZE - 1); // -5.0
    const Handle<MaterialAsset> materials[] = {assets.mat_red, assets.mat_green, assets.mat_blue,
                                               assets.mat_yellow};
    constexpr Vec3 CUBE_EXTENT{0.5f, 0.5f, 0.5f};

    u32 colorIndex = 0;
    for (int row = 0; row < GRID_SIZE; ++row)
    {
        for (int col = 0; col < GRID_SIZE; ++col)
        {
            const f32 x = ORIGIN_OFFSET + SPACING * static_cast<f32>(col);
            const f32 z = ORIGIN_OFFSET + SPACING * static_cast<f32>(row);

            auto cube = world.spawn();
            world.addComponent<Transform>(cube, Transform{{x, 0.0f, z}});
            world.addComponent<GlobalTransform>(cube, GlobalTransform{});
            world.addComponent<MeshAssetRef>(cube, MeshAssetRef{assets.cube_mesh});
            world.addComponent<MaterialAssetRef>(cube, MaterialAssetRef{materials[colorIndex % 4]});
            world.addComponent<AABB>(cube, AABB::fromCenterExtent(Vec3{x, 0.0f, z}, CUBE_EXTENT));
            ++colorIndex;

            // Stack a second cube on top of the first cube of every even row.
            if (col == 0 && row % 2 == 0)
            {
                auto stacked = world.spawn();
                world.addComponent<Transform>(stacked, Transform{{x, 1.0f, z}});
                world.addComponent<GlobalTransform>(stacked, GlobalTransform{});
                world.addComponent<MeshAssetRef>(stacked, MeshAssetRef{assets.cube_mesh});
                world.addComponent<MaterialAssetRef>(stacked, MaterialAssetRef{materials[colorIndex % 4]});
                world.addComponent<AABB>(stacked, AABB::fromCenterExtent(Vec3{x, 1.0f, z}, CUBE_EXTENT));
                ++colorIndex;
            }
        }
    }
}

} // namespace game
