#pragma once
#include "foundation_types.h"
#include "dynamic_array.h"
#include "allocator.h"

namespace Entelechy {

// Sparse-dense set with paged sparse array.
// Avoids linear sparse table blow-up for large EntityIDs.
// O(1) add / remove (swap-and-pop) / has / indexOf.
class SparseSet {
public:
    static constexpr u32 PAGE_SIZE = 1024;
    static constexpr u32 INVALID = 0xFFFFFFFFu;

    SparseSet() = default;
    ~SparseSet();

    SparseSet(const SparseSet&) = delete;
    SparseSet& operator=(const SparseSet&) = delete;

    SparseSet(SparseSet&& other) noexcept;
    SparseSet& operator=(SparseSet&& other) noexcept;

    [[nodiscard]] bool has(u32 id) const;
    void add(u32 id);
    void remove(u32 id);
    [[nodiscard]] u32 indexOf(u32 id) const; // dense index, or INVALID
    [[nodiscard]] u32 count() const { return static_cast<u32>(m_dense.size()); }
    [[nodiscard]] bool empty() const { return m_dense.empty(); }

    [[nodiscard]] const u32* denseData() const { return m_dense.data(); }
    [[nodiscard]] u32* denseData() { return m_dense.data(); }

    [[nodiscard]] u32 denseAt(u32 index) const { return m_dense[index]; }

private:
    void ensurePage(u32 id);
    void freePages();

    DynamicArray<u32*> m_pages;
    DynamicArray<u32> m_dense; // compact entity IDs
};

} // namespace Entelechy
