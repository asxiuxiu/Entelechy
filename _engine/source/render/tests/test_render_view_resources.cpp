#include "test/test_framework.h"
#include "render/render_world/RenderWorld.h"
#include "render/components/RenderCamera.h"
#include "render/culling/ViewVisibleList.h"
#include "render/phase/RenderResources.h"
#include "render/culling/FrustumCullSystem.h"
#include "render/queue/QueueDrawsSystem.h"
#include "core/math/mat4.h"
#include "core/math/frustum.h"
#include "ecs/query/query.h"

using namespace Entelechy;

namespace {

// Minimal stand-in for ExtractCameraSystem that does not require a window.
class MockExtractCameraSystem : public IExtractSystem {
public:
    void extract(const World& /*mainWorld*/, World& renderWorld,
                 FrameArena& /*arena*/, f32 /*dt*/) override {
        // Since RenderWorld::clear() destroys everything each frame, the view
        // entity is recreated here. In addition to ExtractedView we pre-bind the
        // downstream view resources so that Cull/Queue systems can find them on
        // the same entity.
        Entity viewEntity = renderWorld.spawn();

        ExtractedView view{};
        view.view_matrix = Mat4::identity();
        view.proj_matrix = Mat4::identity();
        view.frustum = Frustum::fromMatrix(Mat4::identity());
        view.viewport = Rect{0.0f, 0.0f, 800.0f, 600.0f};
        renderWorld.addComponent(viewEntity, view);

        renderWorld.addComponent(viewEntity, ViewVisibleList{});
        renderWorld.addComponent(viewEntity, ViewBinnedPhases{});
        renderWorld.addComponent(viewEntity, ViewSortedPhases{});
    }
};

} // namespace

TEST(RenderViewResources, SingleViewEntityAfterFullPipeline) {
    RenderWorld renderWorld;
    renderWorld.extractSchedule().registerSystem(new MockExtractCameraSystem());

    World mainWorld;
    renderWorld.extract(mainWorld, 0.016f);

    FrustumCullSystem cullSystem;
    QueueDrawsSystem queueSystem;
    cullSystem.run(renderWorld.world());
    queueSystem.run(renderWorld.world());

    // The entire view state must live on exactly one entity.
    ASSERT_EQ(renderWorld.world().entityCount(), 1u);

    usize combinedCount = 0;
    Query<ExtractedView, ViewVisibleList, ViewBinnedPhases, ViewSortedPhases> combinedQuery(renderWorld.world());
    for (auto [e, ev, vl, bp, sp] : combinedQuery) {
        ++combinedCount;
        ASSERT_TRUE(ev != nullptr);
        ASSERT_TRUE(vl != nullptr);
        ASSERT_TRUE(bp != nullptr);
        ASSERT_TRUE(sp != nullptr);
    }
    ASSERT_EQ(combinedCount, 1u);
}

TEST(RenderViewResources, TwoExtractFramesStillOneEntity) {
    RenderWorld renderWorld;
    renderWorld.extractSchedule().registerSystem(new MockExtractCameraSystem());

    World mainWorld;
    FrustumCullSystem cullSystem;
    QueueDrawsSystem queueSystem;

    renderWorld.extract(mainWorld, 0.016f);
    cullSystem.run(renderWorld.world());
    queueSystem.run(renderWorld.world());
    ASSERT_EQ(renderWorld.world().entityCount(), 1u);

    renderWorld.extract(mainWorld, 0.016f);
    cullSystem.run(renderWorld.world());
    queueSystem.run(renderWorld.world());
    ASSERT_EQ(renderWorld.world().entityCount(), 1u);
}
