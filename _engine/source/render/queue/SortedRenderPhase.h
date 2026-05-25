#pragma once
#include "foundation_types.h"
#include "dynamic_array.h"
#include "render/queue/PhaseItem.h"

namespace Entelechy {

// SortedRenderPhase — back-to-front sorted PhaseItems.
// Used for Transparent3D and UI phases.
class SortedRenderPhase {
    DynamicArray<PhaseItem> m_items;
public:
    void addItem(const PhaseItem& item);
    void prepare(); // sort by sort_key.value ascending
    void clear();
    const DynamicArray<PhaseItem>& getItems() const { return m_items; }
};

} // namespace Entelechy
