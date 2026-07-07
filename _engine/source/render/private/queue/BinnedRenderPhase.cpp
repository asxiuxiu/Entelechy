#include "render/queue/BinnedRenderPhase.h"

namespace Entelechy
{

void BinnedRenderPhase::addItem(const PhaseItem &item)
{
    u32 binKey = item.sort_key.packed.material_id;
    if (auto *index = m_bin_index.find(binKey))
    {
        m_bins[*index].items.pushBack(item);
        return;
    }
    usize newIndex = m_bins.size();
    PhaseBin newBin;
    newBin.material_id = binKey;
    newBin.items.pushBack(item);
    m_bins.pushBack(std::move(newBin));
    m_bin_index.insert(binKey, newIndex);
}

void BinnedRenderPhase::clear()
{
    m_bins.clear();
    m_bin_index.clear();
}

} // namespace Entelechy
