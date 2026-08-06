#pragma once
#include "render_system/queue/binned_render_phase.h"
#include "render_system/queue/sorted_render_phase.h"

namespace Entelechy
{

// ViewBinnedPhases — output of Queue stage for Opaque / AlphaMask.
// One per view. Binned by material_id to reduce state changes.
struct ViewBinnedPhases
{
    BinnedRenderPhase opaque;
    BinnedRenderPhase alpha_mask;
};

// ViewSortedPhases — output of Queue stage for Transparent / UI.
// One per view. Sorted back-to-front by depth.
struct ViewSortedPhases
{
    SortedRenderPhase transparent;
    SortedRenderPhase ui;
};

} // namespace Entelechy
