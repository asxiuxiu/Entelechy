#pragma once
#include "render/queue/BinnedRenderPhase.h"
#include "render/queue/SortedRenderPhase.h"
#include "ecs/type/type_registry.h"

namespace Entelechy {

// ViewBinnedPhases — output of Queue stage for Opaque / AlphaMask.
// One per view. Binned by material_id to reduce state changes.
struct ViewBinnedPhases {
    BinnedRenderPhase opaque;
    BinnedRenderPhase alpha_mask;
};

REFLECT_COMPONENT(ViewBinnedPhases)

// ViewSortedPhases — output of Queue stage for Transparent / UI.
// One per view. Sorted back-to-front by depth.
struct ViewSortedPhases {
    SortedRenderPhase transparent;
    SortedRenderPhase ui;
};

REFLECT_COMPONENT(ViewSortedPhases)

} // namespace Entelechy
