#include "render/culling/FrustumCullSystem.h"
#include "render/components/RenderCamera.h"
#include "render/components/RenderComponents.h"
#include "core/math/aabb.h"
#include "render/culling/ViewVisibleList.h"
#include "ecs/query/query.h"

namespace Entelechy {

void FrustumCullSystem::run(World& renderWorld) {
    // Find the single view entity.
    Entity viewEntity{0, 0};
    const ExtractedView* view = nullptr;
    Query<ExtractedView> viewQuery(renderWorld);
    for (auto [ve, ev] : viewQuery) {
        viewEntity = ve;
        view = ev;
        break;
    }
    if (!view) return;

    // Ensure ViewVisibleList is attached to the same view entity.
    ViewVisibleList* visibleList = renderWorld.getComponent<ViewVisibleList>(viewEntity);
    if (!visibleList) {
        renderWorld.addComponent(viewEntity, ViewVisibleList{});
        visibleList = renderWorld.getComponent<ViewVisibleList>(viewEntity);
    }
    visibleList->entities.clear();

    // Iterate all render-world entities with RenderTransform.
    Query<RenderTransform> transformQuery(renderWorld);
    for (auto [entity, transform] : transformQuery) {
        const AABB* aabb = renderWorld.getComponent<AABB>(entity);
        bool visible = true;
        if (aabb) {
            visible = view->frustum.intersectsAABB(*aabb);
        }
        if (visible) {
            visibleList->entities.pushBack(entity);
        }
    }
}

} // namespace Entelechy
