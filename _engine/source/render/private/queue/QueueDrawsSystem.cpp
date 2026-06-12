#include "render/queue/QueueDrawsSystem.h"
#include "render/components/RenderComponents.h"
#include "render/components/RenderCamera.h"
#include "render/culling/ViewVisibleList.h"
#include "render/phase/RenderResources.h"
#include "ecs/query/query.h"
#include <cstring>

namespace Entelechy {

static u32 floatBitsToUint(f32 f) {
    u32 u;
    std::memcpy(&u, &f, sizeof(f32));
    return u;
}

void QueueDrawsSystem::run(World& renderWorld) {
    // Locate the single view entity (for depth calculation and resource lookup).
    Entity viewEntity{0, 0};
    const ExtractedView* view = nullptr;
    ConstQuery<ExtractedView> viewQuery(renderWorld);
    for (auto [ve, ev] : viewQuery) {
        viewEntity = ve;
        view = ev;
        break;
    }
    if (!view) return;

    // Ensure phase containers are attached to the same view entity.
    ViewBinnedPhases* binned = renderWorld.getComponent<ViewBinnedPhases>(viewEntity);
    if (!binned) {
        renderWorld.addComponent(viewEntity, ViewBinnedPhases{});
        binned = renderWorld.getComponent<ViewBinnedPhases>(viewEntity);
    }
    binned->opaque.clear();
    binned->alpha_mask.clear();

    ViewSortedPhases* sorted = renderWorld.getComponent<ViewSortedPhases>(viewEntity);
    if (!sorted) {
        renderWorld.addComponent(viewEntity, ViewSortedPhases{});
        sorted = renderWorld.getComponent<ViewSortedPhases>(viewEntity);
    }
    sorted->transparent.clear();
    sorted->ui.clear();

    // Get the visible list from the same view entity.
    const ViewVisibleList* visibleList = renderWorld.getComponent<ViewVisibleList>(viewEntity);
    if (!visibleList) return;

    Mat4 viewMatrix = view->view_matrix;

    for (usize i = 0; i < visibleList->entities.size(); ++i) {
        Entity entity = visibleList->entities[i];

        const RenderTransform* transform = renderWorld.getComponent<RenderTransform>(entity);
        const RenderMaterial* material = renderWorld.getComponent<RenderMaterial>(entity);
        if (!transform || !material) continue;

        // Compute depth in view space (positive = in front of camera).
        Vec3 worldPos = transform->world_matrix.transformPoint(Vec3{0.0f, 0.0f, 0.0f});
        Vec3 viewPos = viewMatrix.transformPoint(worldPos);
        f32 depth = viewPos.z;

        // Pack depth into sort key bits.
        u32 depthBits = floatBitsToUint(depth);

        RenderPhase phase = material->render_phase;
        SortKey key{};
        key.packed.material_id = static_cast<u16>(material->material_asset_id & 0xFFFFu);
        key.packed.phase = static_cast<u8>(phase);
        key.packed.reserved = 0;

        PhaseItem item{};
        item.render_entity = entity;
        item.instance_count = 1;

        switch (phase) {
            case RenderPhase::ShadowMap:
                // Not handled in this simplified checkpoint.
                break;
            case RenderPhase::Opaque3D:
                key.packed.depth = depthBits; // ascending
                item.sort_key = key;
                binned->opaque.addItem(item);
                break;
            case RenderPhase::AlphaMask:
                key.packed.depth = depthBits; // ascending
                item.sort_key = key;
                binned->alpha_mask.addItem(item);
                break;
            case RenderPhase::Transparent3D:
                key.packed.depth = ~depthBits; // descending (invert for sort ascending)
                item.sort_key = key;
                sorted->transparent.addItem(item);
                break;
            case RenderPhase::UI:
                key.packed.depth = ~depthBits; // descending
                item.sort_key = key;
                sorted->ui.addItem(item);
                break;
        }
    }

    // Prepare sorted phases.
    sorted->transparent.prepare();
    sorted->ui.prepare();
}

} // namespace Entelechy
