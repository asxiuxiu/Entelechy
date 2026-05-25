#include "queue/QueueDrawsSystem.h"
#include "components/RenderComponents.h"
#include "components/RenderCamera.h"
#include "culling/ViewVisibleList.h"
#include "render/RenderResources.h"
#include "query.h"
#include <cstring>

namespace Entelechy {

static u32 floatBitsToUint(f32 f) {
    u32 u;
    std::memcpy(&u, &f, sizeof(f32));
    return u;
}

void QueueDrawsSystem::run(World& renderWorld) {
    // Locate the view (for depth calculation).
    const ExtractedView* view = nullptr;
    ConstQuery<ExtractedView> viewQuery(renderWorld);
    for (auto [ve, ev] : viewQuery) {
        view = ev;
        break;
    }
    if (!view) return;

    // Locate or create the phase containers.
    ViewBinnedPhases* binned = nullptr;
    ViewSortedPhases* sorted = nullptr;

    Query<ViewBinnedPhases> bq(renderWorld);
    for (auto [be, bp] : bq) {
        binned = bp;
        break;
    }
    if (!binned) {
        Entity e = renderWorld.spawn();
        renderWorld.addComponent(e, ViewBinnedPhases{});
        binned = renderWorld.getComponent<ViewBinnedPhases>(e);
    }
    binned->opaque.clear();
    binned->alpha_mask.clear();

    Query<ViewSortedPhases> sq(renderWorld);
    for (auto [se, sp] : sq) {
        sorted = sp;
        break;
    }
    if (!sorted) {
        Entity e = renderWorld.spawn();
        renderWorld.addComponent(e, ViewSortedPhases{});
        sorted = renderWorld.getComponent<ViewSortedPhases>(e);
    }
    sorted->transparent.clear();
    sorted->ui.clear();

    // Locate the visible list.
    ConstQuery<ViewVisibleList> vlQuery(renderWorld);
    const ViewVisibleList* visibleList = nullptr;
    for (auto [ve, vl] : vlQuery) {
        visibleList = vl;
        break;
    }
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
