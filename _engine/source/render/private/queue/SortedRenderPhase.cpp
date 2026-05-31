#include "render/queue/SortedRenderPhase.h"
#include <algorithm>

namespace Entelechy {

void SortedRenderPhase::addItem(const PhaseItem& item) {
    m_items.pushBack(item);
}

void SortedRenderPhase::prepare() {
    std::sort(m_items.begin(), m_items.end(), [](const PhaseItem& a, const PhaseItem& b) {
        return a.sort_key.value < b.sort_key.value;
    });
}

void SortedRenderPhase::clear() {
    m_items.clear();
}

} // namespace Entelechy
