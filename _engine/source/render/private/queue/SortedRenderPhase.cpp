#include "render/queue/SortedRenderPhase.h"
#include "core/algorithm/radix_sort.h"

namespace Entelechy
{

void SortedRenderPhase::addItem(const PhaseItem &item)
{
    m_items.pushBack(item);
}

void SortedRenderPhase::prepare()
{
    if (m_items.size() > 1)
    {
        m_scratch.resize(m_items.size());
        radixSort64(
            m_items.data(), m_items.size(), [](const PhaseItem &item) -> u64 { return item.sort_key.value; },
            m_scratch.data());
    }
}

void SortedRenderPhase::clear()
{
    m_items.clear();
}

} // namespace Entelechy
