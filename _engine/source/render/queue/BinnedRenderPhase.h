#pragma once
#include "foundation_types.h"
#include "dynamic_array.h"
#include "render/queue/PhaseItem.h"

namespace Entelechy {

struct PhaseBin {
    u32 material_id = 0;
    DynamicArray<PhaseItem> items;
};

// BinnedRenderPhase — groups PhaseItems by material_id to reduce state changes.
// Used for Opaque3D and AlphaMask phases.
class BinnedRenderPhase {
    DynamicArray<PhaseBin> m_bins;
public:
    void addItem(const PhaseItem& item);
    void clear();
    const DynamicArray<PhaseBin>& getBins() const { return m_bins; }
};

} // namespace Entelechy
