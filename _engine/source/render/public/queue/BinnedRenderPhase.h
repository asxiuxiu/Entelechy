#pragma once
#include "core/foundation_types.h"
#include "core/container/dynamic_array.h"
#include "core/container/hash_map.h"
#include "render/queue/PhaseItem.h"

namespace Entelechy
{

struct PhaseBin
{
    u32 material_id = 0;
    DynamicArray<PhaseItem> items;
};

// BinnedRenderPhase — groups PhaseItems by material_id to reduce state changes.
// Used for Opaque3D and AlphaMask phases.
class BinnedRenderPhase
{
    // material_id -> index into m_bins. Keeps addItem O(1) average instead of
    // the previous O(n) linear scan over all bins.
    HashMap<u32, usize> m_bin_index;
    DynamicArray<PhaseBin> m_bins;

public:
    void addItem(const PhaseItem &item);
    void clear();
    const DynamicArray<PhaseBin> &getBins() const
    {
        return m_bins;
    }
};

} // namespace Entelechy
