#include "test/test_framework.h"
#include "render/render_world/RenderWorld.h"
#include "render/components/RenderComponents.h"
#include "render/components/RenderCamera.h"
#include "render/culling/FrustumCullSystem.h"
#include "render/culling/ViewVisibleList.h"
#include "render/phase/RenderResources.h"
#include "render/queue/QueueDrawsSystem.h"
#include "thread_pool/thread_pool.h"
#include "core/math/aabb.h"
#include "core/math/frustum.h"
#include "core/math/mat4.h"
#include "core/math/vec.h"
#include "core/string/string_id.h"
#include "ecs/query/query.h"
#include "ecs/type/type_registry.h"

using namespace Entelechy;

namespace {

class MockExtractCameraSystem : public IExtractSystem {
public:
    void extract(const World& /*mainWorld*/, World& renderWorld,
                 FrameArena& /*arena*/, f32 /*dt*/) override {
        Entity viewEntity = renderWorld.spawn();
        ExtractedView view{};
        view.view_matrix = Mat4::identity();
        view.proj_matrix = Mat4::identity();
        view.frustum = Frustum::fromMatrix(Mat4::identity());
        view.viewport = Rect{0.0f, 0.0f, 800.0f, 600.0f};
        view.near_plane = 0.1f;
        view.far_plane = 100.0f;
        renderWorld.addComponent(viewEntity, view);
        renderWorld.addComponent(viewEntity, ViewVisibleList{});
        renderWorld.addComponent(viewEntity, ViewBinnedPhases{});
        renderWorld.addComponent(viewEntity, ViewSortedPhases{});
    }
};

void setupBoxFrustum(ExtractedView& view) {
    // Axis-aligned box: x in [-10, 10], y in [-10, 10], z in [0, 100].
    view.frustum.planes[Frustum::Left]   = Vec4{ 1.0f,  0.0f,  0.0f, 10.0f};
    view.frustum.planes[Frustum::Right]  = Vec4{-1.0f,  0.0f,  0.0f, 10.0f};
    view.frustum.planes[Frustum::Bottom] = Vec4{ 0.0f,  1.0f,  0.0f, 10.0f};
    view.frustum.planes[Frustum::Top]    = Vec4{ 0.0f, -1.0f,  0.0f, 10.0f};
    view.frustum.planes[Frustum::Near]   = Vec4{ 0.0f,  0.0f,  1.0f,  0.0f};
    view.frustum.planes[Frustum::Far]    = Vec4{ 0.0f,  0.0f, -1.0f, 100.0f};
}

void registerAABBComponent() {
    TypeRegistry& registry = TypeRegistry::instance();
    ComponentTypeID id = registry.getOrAllocateTypeID<AABB>();
    u64 mask = 1ull << id;
    registry.registerComponent(id, mask, makeComponentDesc<AABB>("AABB"_sid, {}));
}

Entity findViewEntity(World& world) {
    Query<ExtractedView> q(world);
    for (auto [e, v] : q) {
        (void)v;
        return e;
    }
    return Entity{0xFFFFFFFFu, 0};
}

void populateRenderables(World& world, usize count) {
    for (usize i = 0; i < count; ++i) {
        Entity e = world.spawn();
        f32 x = static_cast<f32>(i) * 0.01f - 5.0f;
        world.addComponent(e, RenderTransform{Mat4::fromTranslation(Vec3{x, 0.0f, 10.0f})});
        world.addComponent(e, AABB::fromCenterExtent(Vec3{x, 0.0f, 10.0f}, Vec3{0.05f, 0.05f, 0.05f}));

        RenderPhase phase = (i % 2 == 0) ? RenderPhase::Opaque3D : RenderPhase::Transparent3D;
        world.addComponent(e, RenderMaterial{static_cast<u32>(i % 8), phase});
    }
}

usize countBinnedItems(const BinnedRenderPhase& phase) {
    usize total = 0;
    for (const auto& bin : phase.getBins()) {
        total += bin.items.size();
    }
    return total;
}

} // namespace

TEST(RenderParallel, CullingSerialMatchesParallel) {
    RenderWorld serialWorld;
    serialWorld.extractSchedule().registerSystem(new MockExtractCameraSystem());
    World mainWorld;
    serialWorld.extract(mainWorld, 0.016f);

    World& world = serialWorld.world();
    Entity viewEntity = findViewEntity(world);
    ASSERT_TRUE(viewEntity.valid());
    setupBoxFrustum(*world.getComponent<ExtractedView>(viewEntity));

    registerAABBComponent();
    populateRenderables(world, 1024);

    FrustumCullSystem serial;
    serial.run(world);

    const ViewVisibleList* serialList = world.getComponent<ViewVisibleList>(viewEntity);
    ASSERT_TRUE(serialList != nullptr);
    DynamicArray<Entity> serialVisible = serialList->entities;

    // Clear and run again with a thread pool.
    ViewVisibleList* parallelList = world.getComponent<ViewVisibleList>(viewEntity);
    parallelList->entities.clear();

    ThreadPool pool(4);
    FrustumCullSystem parallel(&pool);
    parallel.run(world);

    ASSERT_EQ(parallelList->entities.size(), serialVisible.size());
    for (usize i = 0; i < serialVisible.size(); ++i) {
        ASSERT_EQ(parallelList->entities[i], serialVisible[i]);
    }
}

TEST(RenderParallel, QueueDrawsSerialMatchesParallel) {
    RenderWorld serialWorld;
    RenderWorld parallelWorld;
    serialWorld.extractSchedule().registerSystem(new MockExtractCameraSystem());
    parallelWorld.extractSchedule().registerSystem(new MockExtractCameraSystem());

    World mainWorld;
    serialWorld.extract(mainWorld, 0.016f);
    parallelWorld.extract(mainWorld, 0.016f);

    World& serialRender = serialWorld.world();
    World& parallelRender = parallelWorld.world();

    Entity serialView = findViewEntity(serialRender);
    Entity parallelView = findViewEntity(parallelRender);
    ASSERT_TRUE(serialView.valid());
    ASSERT_TRUE(parallelView.valid());

    setupBoxFrustum(*serialRender.getComponent<ExtractedView>(serialView));
    setupBoxFrustum(*parallelRender.getComponent<ExtractedView>(parallelView));

    registerAABBComponent();
    populateRenderables(serialRender, 1024);
    populateRenderables(parallelRender, 1024);

    // Serial pipeline.
    FrustumCullSystem serialCull;
    QueueDrawsSystem serialQueue;
    serialCull.run(serialRender);
    serialQueue.run(serialRender);

    const ViewBinnedPhases* serialBinned = serialRender.getComponent<ViewBinnedPhases>(serialView);
    const ViewSortedPhases* serialSorted = serialRender.getComponent<ViewSortedPhases>(serialView);
    ASSERT_TRUE(serialBinned != nullptr);
    ASSERT_TRUE(serialSorted != nullptr);

    // Parallel pipeline.
    ThreadPool pool(4);
    FrustumCullSystem parallelCull(&pool);
    QueueDrawsSystem parallelQueue(&pool);
    parallelCull.run(parallelRender);
    parallelQueue.run(parallelRender);

    const ViewBinnedPhases* parallelBinned = parallelRender.getComponent<ViewBinnedPhases>(parallelView);
    const ViewSortedPhases* parallelSorted = parallelRender.getComponent<ViewSortedPhases>(parallelView);
    ASSERT_TRUE(parallelBinned != nullptr);
    ASSERT_TRUE(parallelSorted != nullptr);

    // Compare totals.
    ASSERT_EQ(countBinnedItems(parallelBinned->opaque), countBinnedItems(serialBinned->opaque));
    ASSERT_EQ(countBinnedItems(parallelBinned->alpha_mask), countBinnedItems(serialBinned->alpha_mask));
    ASSERT_EQ(parallelSorted->transparent.getItems().size(), serialSorted->transparent.getItems().size());
    ASSERT_EQ(parallelSorted->ui.getItems().size(), serialSorted->ui.getItems().size());

    // Compare sorted transparent output order.
    const auto& serialTransparent = serialSorted->transparent.getItems();
    const auto& parallelTransparent = parallelSorted->transparent.getItems();
    for (usize i = 0; i < serialTransparent.size(); ++i) {
        ASSERT_EQ(parallelTransparent[i].sort_key.value, serialTransparent[i].sort_key.value);
        ASSERT_EQ(parallelTransparent[i].render_entity, serialTransparent[i].render_entity);
    }
}
