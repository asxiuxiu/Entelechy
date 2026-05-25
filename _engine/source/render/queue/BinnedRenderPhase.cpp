#include "queue/BinnedRenderPhase.h"

namespace Entelechy {

void BinnedRenderPhase::addItem(const PhaseItem& item) {
    u32 binKey = item.sort_key.packed.material_id;
    for (usize i = 0; i < m_bins.size(); ++i) {
        if (m_bins[i].material_id == binKey) {
            m_bins[i].items.pushBack(item);
            return;
        }
    }
    PhaseBin newBin;
    newBin.material_id = binKey;
    newBin.items.pushBack(item);
    m_bins.pushBack(std::move(newBin));
}

void BinnedRenderPhase::clear() {
    m_bins.clear();
}

} // namespace Entelechy
