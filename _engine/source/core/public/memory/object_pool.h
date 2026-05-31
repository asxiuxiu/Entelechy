#pragma once
#include "core/foundation_types.h"
#include "core/allocator/iallocator.h"
#include "core/container/dynamic_array.h"
#include "core/allocator/allocator.h"
#include <memory>
#include <utility>

namespace Entelechy {

// Handle to an object inside an ObjectPool.
// generation == 0 means invalid.
struct PoolHandle {
    u32 slotIndex = 0xFFFFFFFFu;
    u32 generation = 0;

    [[nodiscard]] bool valid() const { return generation != 0; }
    [[nodiscard]] bool operator==(const PoolHandle& other) const = default;
};

// Dynamic object pool: grows by appending fixed-size blocks (1024 slots each).
// Returns stable PoolHandles with generational safety.
// Also implements IAllocator for single-T allocations (size must <= sizeof(T)).
//
// Allocation decision guide: see ALLOCATOR_GUIDE.md
template <typename T>
class ObjectPool : public IAllocator {
public:
    static constexpr usize SLOTS_PER_BLOCK = 1024;

    explicit ObjectPool(usize initialBlockCount = 1);
    ~ObjectPool();

    // Typed API -------------------------------------------------------
    template <typename... Args>
    [[nodiscard]] PoolHandle alloc(Args&&... args);
    void free(PoolHandle handle);
    void free(T* obj);

    [[nodiscard]] bool isValid(PoolHandle handle) const;
    [[nodiscard]] T* resolve(PoolHandle handle);
    [[nodiscard]] const T* resolve(PoolHandle handle) const;

    // IAllocator interface (single T object only) ---------------------
    void* allocate(usize size, usize align) override;
    void free(void* ptr) override;
    [[nodiscard]] AllocatorStats getStats() const override;

    [[nodiscard]] bool empty() const { return m_count == 0; }
    [[nodiscard]] usize count() const { return m_count; }
    [[nodiscard]] usize capacity() const { return m_blocks.size() * SLOTS_PER_BLOCK; }

private:
    struct Slot {
        union {
            T object;
            struct {
                u32 nextFreeIndex;
                u32 generation;
            } free;
        };
        bool occupied = false;

        Slot() {}
        ~Slot() {}
    };

    struct Block {
        Slot* slots;
    };

    void addBlock();
    Slot& getSlot(u32 globalIndex);
    const Slot& getSlot(u32 globalIndex) const;

    DynamicArray<Block> m_blocks;
    u32 m_free_head;
    usize m_count;
    mutable AllocatorStats m_stats;

    static constexpr u32 INVALID_SLOT = 0xFFFFFFFFu;
};

// ------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------

template <typename T>
ObjectPool<T>::ObjectPool(usize initialBlockCount)
    : m_free_head(INVALID_SLOT)
    , m_count(0) {
    if (initialBlockCount == 0) initialBlockCount = 1;
    for (usize i = 0; i < initialBlockCount; ++i) {
        addBlock();
    }
}

template <typename T>
ObjectPool<T>::~ObjectPool() {
    for (usize b = 0; b < m_blocks.size(); ++b) {
        for (usize i = 0; i < SLOTS_PER_BLOCK; ++i) {
            Slot& slot = m_blocks[b].slots[i];
            if (slot.occupied) {
                std::destroy_at(&slot.object);
            }
        }
        DefaultAllocator::free(m_blocks[b].slots);
    }
}

template <typename T>
void ObjectPool<T>::addBlock() {
    Block block;
    block.slots = static_cast<Slot*>(DefaultAllocator::alloc(SLOTS_PER_BLOCK * sizeof(Slot), alignof(Slot)));
    u32 base = static_cast<u32>(m_blocks.size() * SLOTS_PER_BLOCK);

    for (usize i = SLOTS_PER_BLOCK; i > 0; --i) {
        u32 localIdx = static_cast<u32>(i - 1);
        block.slots[localIdx].free.nextFreeIndex = m_free_head;
        block.slots[localIdx].free.generation = 1; // 0 = invalid
        block.slots[localIdx].occupied = false;
        m_free_head = base + localIdx;
    }

    m_blocks.pushBack(block);
}

template <typename T>
typename ObjectPool<T>::Slot& ObjectPool<T>::getSlot(u32 globalIndex) {
    u32 blockIdx = globalIndex / SLOTS_PER_BLOCK;
    u32 localIdx = globalIndex % SLOTS_PER_BLOCK;
    return m_blocks[blockIdx].slots[localIdx];
}

template <typename T>
const typename ObjectPool<T>::Slot& ObjectPool<T>::getSlot(u32 globalIndex) const {
    u32 blockIdx = globalIndex / SLOTS_PER_BLOCK;
    u32 localIdx = globalIndex % SLOTS_PER_BLOCK;
    return m_blocks[blockIdx].slots[localIdx];
}

template <typename T>
template <typename... Args>
PoolHandle ObjectPool<T>::alloc(Args&&... args) {
    if (m_free_head == INVALID_SLOT) {
        addBlock();
    }
    u32 index = m_free_head;
    Slot& slot = getSlot(index);
    m_free_head = slot.free.nextFreeIndex;
    std::construct_at(&slot.object, std::forward<Args>(args)...);
    slot.occupied = true;
    ++m_count;
    m_stats.allocationCount++;
    return PoolHandle{index, slot.free.generation};
}

template <typename T>
void ObjectPool<T>::free(PoolHandle handle) {
    if (!isValid(handle)) return;
    Slot& slot = getSlot(handle.slotIndex);
    std::destroy_at(&slot.object);
    slot.occupied = false;
    ++slot.free.generation;
    slot.free.nextFreeIndex = m_free_head;
    m_free_head = handle.slotIndex;
    --m_count;
}

template <typename T>
void ObjectPool<T>::free(T* obj) {
    if (!obj) return;
    // Reverse lookup via pointer arithmetic (O(blockCount), acceptable for small pools)
    for (usize i = 0; i < m_blocks.size(); ++i) {
        Slot* slots = m_blocks[i].slots;
        if (obj >= &slots[0].object && obj < &slots[SLOTS_PER_BLOCK].object) {
            usize localIdx = static_cast<usize>(reinterpret_cast<Slot*>(obj) - slots);
            u32 globalIdx = static_cast<u32>(i * SLOTS_PER_BLOCK + localIdx);
            free(PoolHandle{globalIdx, slots[localIdx].free.generation});
            return;
        }
    }
    CHECK(false && "ObjectPool::free(T*) pointer not found in any block");
}

template <typename T>
bool ObjectPool<T>::isValid(PoolHandle handle) const {
    if (handle.slotIndex >= capacity()) return false;
    const Slot& slot = getSlot(handle.slotIndex);
    return slot.occupied && slot.free.generation == handle.generation;
}

template <typename T>
T* ObjectPool<T>::resolve(PoolHandle handle) {
    if (!isValid(handle)) return nullptr;
    return &getSlot(handle.slotIndex).object;
}

template <typename T>
const T* ObjectPool<T>::resolve(PoolHandle handle) const {
    if (!isValid(handle)) return nullptr;
    return &getSlot(handle.slotIndex).object;
}

template <typename T>
void* ObjectPool<T>::allocate(usize size, usize align) {
    (void)align;
    if (size > sizeof(T)) return nullptr;
    PoolHandle h = alloc();
    return resolve(h);
}

template <typename T>
void ObjectPool<T>::free(void* ptr) {
    free(static_cast<T*>(ptr));
}

template <typename T>
AllocatorStats ObjectPool<T>::getStats() const {
    AllocatorStats result = m_stats;
    result.totalAllocated = m_count * sizeof(T);
    result.activeCount = m_count;
    if (result.totalAllocated > m_stats.peakAllocated) {
        m_stats.peakAllocated = result.totalAllocated;
    }
    result.peakAllocated = m_stats.peakAllocated;
    return result;
}

} // namespace Entelechy
