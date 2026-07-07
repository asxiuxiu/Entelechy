#pragma once
#include "core/foundation_types.h"
#include "core/container/dynamic_array.h"
#include "render/queue/PhaseItem.h"

namespace Entelechy
{

// SortedRenderPhase — back-to-front sorted PhaseItems.
// Used for Transparent3D and UI phases.
class SortedRenderPhase
{
    DynamicArray<PhaseItem> m_items;
    DynamicArray<PhaseItem> m_scratch; // temp buffer for radix sort
public:
    void addItem(const PhaseItem &item);
    void prepare(); // stable radix sort by sort_key.value ascending
    void clear();
    const DynamicArray<PhaseItem> &getItems() const
    {
        return m_items;
    }
};

} // namespace Entelechy
