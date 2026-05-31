#include "render/culling/FrustumCullSystem.h"
#include "render/components/RenderCamera.h"
#include "render/components/RenderComponents.h"
#include "core/math/aabb.h"
#include "render/culling/ViewVisibleList.h"
#include "ecs/query/query.h"

namespace Entelechy {

void FrustumCullSystem::run(World& renderWorld) {
    // Find the view entity and its frustum.
    const ExtractedView* view = nullptr;
    Query<ExtractedView> viewQuery(renderWorld);
    for (auto [ve, ev] : viewQuery) {
        view = ev;
        break;
    }
    if (!view) return;

    // Find or create ViewVisibleList on a dedicated entity.
    ViewVisibleList* visibleList = nullptr;
    Query<ViewVisibleList> vlQuery(renderWorld);
    for (auto [ve, vl] : vlQuery) {
        visibleList = vl;
        break;
    }
    if (!visibleList) {
        Entity vlEntity = renderWorld.spawn();
        renderWorld.addComponent(vlEntity, ViewVisibleList{});
        visibleList = renderWorld.getComponent<ViewVisibleList>(vlEntity);
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
