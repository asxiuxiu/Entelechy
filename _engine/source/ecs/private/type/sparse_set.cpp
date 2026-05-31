#include "ecs/type/sparse_set.h"

namespace Entelechy {

SparseSet::~SparseSet() {
    freePages();
}

SparseSet::SparseSet(SparseSet&& other) noexcept
    : m_pages(std::move(other.m_pages))
    , m_dense(std::move(other.m_dense)) {
}

SparseSet& SparseSet::operator=(SparseSet&& other) noexcept {
    if (this != &other) {
        freePages();
        m_pages = std::move(other.m_pages);
        m_dense = std::move(other.m_dense);
    }
    return *this;
}

bool SparseSet::has(u32 id) const {
    u32 pageIdx = id / PAGE_SIZE;
    if (pageIdx >= static_cast<u32>(m_pages.size())) return false;
    u32* page = m_pages[pageIdx];
    if (!page) return false;
    return page[id % PAGE_SIZE] != INVALID;
}

void SparseSet::add(u32 id) {
    if (has(id)) return;
    ensurePage(id);
    u32 pageIdx = id / PAGE_SIZE;
    u32 offset = id % PAGE_SIZE;
    u32 denseIdx = static_cast<u32>(m_dense.size());
    m_pages[pageIdx][offset] = denseIdx;
    m_dense.pushBack(id);
}

void SparseSet::remove(u32 id) {
    if (!has(id)) return;
    u32 pageIdx = id / PAGE_SIZE;
    u32 offset = id % PAGE_SIZE;
    u32 denseIdx = m_pages[pageIdx][offset];
    u32 lastIdx = static_cast<u32>(m_dense.size()) - 1;
    u32 lastId = m_dense[lastIdx];

    // swap-and-pop dense
    m_dense[denseIdx] = lastId;
    m_pages[lastId / PAGE_SIZE][lastId % PAGE_SIZE] = denseIdx;
    m_dense.popBack();
    m_pages[pageIdx][offset] = INVALID;
}

u32 SparseSet::indexOf(u32 id) const {
    if (!has(id)) return INVALID;
    return m_pages[id / PAGE_SIZE][id % PAGE_SIZE];
}

void SparseSet::ensurePage(u32 id) {
    u32 pageIdx = id / PAGE_SIZE;
    if (pageIdx >= static_cast<u32>(m_pages.size())) {
        u32 oldSize = static_cast<u32>(m_pages.size());
        m_pages.resize(pageIdx + 1);
        for (u32 i = oldSize; i <= pageIdx; ++i) {
            m_pages[i] = nullptr;
        }
    }
    if (!m_pages[pageIdx]) {
        u32* page = static_cast<u32*>(DefaultAllocator::alloc(PAGE_SIZE * sizeof(u32), alignof(u32)));
        for (u32 i = 0; i < PAGE_SIZE; ++i) {
            page[i] = INVALID;
        }
        m_pages[pageIdx] = page;
    }
}

void SparseSet::freePages() {
    for (usize i = 0; i < m_pages.size(); ++i) {
        if (m_pages[i]) {
            DefaultAllocator::free(m_pages[i]);
        }
    }
    m_pages.clear();
}

} // namespace Entelechy
