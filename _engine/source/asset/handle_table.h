#pragma once
#include "asset_handle.h"
#include "dynamic_array.h"
#include "foundation_types.h"
#include <utility>

namespace Entelechy {

// ------------------------------------------------------------------
// HandleTableSlot<T> — one slot in the dense asset table
// ------------------------------------------------------------------
template<typename T>
struct HandleTableSlot {
    T data{};
    u32 generation = 0;
    bool occupied = false;
};

// ------------------------------------------------------------------
// HandleTable<T> — dense storage with ABA protection and ref counts
// ------------------------------------------------------------------
// Assets are stored in a contiguous array for cache locality.
// Free slots are tracked by a free-list for O(1) reuse.
//
// Reference counting is kept in a parallel array. This design is
// ECS-friendly: ref counts can be updated in bulk by Systems.
//
// NOTE: T must be default-constructible because DynamicArray
// resize() default-constructs elements.
// ------------------------------------------------------------------
template<typename T>
class HandleTable {
public:
    // Allocate a slot and return its handle.
    // The slot is reserved but not yet occupied (get() returns nullptr
    // until fill() is called). This supports async loading workflows.
    Handle<T> allocate() {
        if (!m_freeList.empty()) {
            u32 index = m_freeList.back();
            m_freeList.popBack();
            auto& slot = m_slots[index];
            slot.generation++;
            m_refCounts[index] = 0;
            return Handle<T>{index, slot.generation};
        }
        u32 index = static_cast<u32>(m_slots.size());
        m_slots.resize(index + 1);
        m_refCounts.resize(index + 1);
        auto& slot = m_slots[index];
        slot.generation = 1;
        m_refCounts[index] = 0;
        return Handle<T>{index, 1};
    }

    // Release a slot. Generation is bumped, invalidating old handles.
    void release(Handle<T> handle) {
        if (!handle.valid()) return;
        if (handle.index >= m_slots.size()) return;
        auto& slot = m_slots[handle.index];
        if (slot.generation != handle.generation) return;
        if (slot.occupied) {
            slot.occupied = false;
            --m_activeCount;
        }
        slot.generation++;
        std::destroy_at(&slot.data);
        std::construct_at(&slot.data);
        m_refCounts[handle.index] = 0;
        m_freeList.pushBack(handle.index);
    }

    // Try to get a pointer to the data. Returns nullptr if handle
    // is stale or the slot has not been filled yet.
    [[nodiscard]] T* tryGet(Handle<T> handle) {
        if (!handle.valid()) return nullptr;
        if (handle.index >= m_slots.size()) return nullptr;
        auto& slot = m_slots[handle.index];
        if (!slot.occupied || slot.generation != handle.generation) return nullptr;
        return &slot.data;
    }

    [[nodiscard]] const T* tryGet(Handle<T> handle) const {
        if (!handle.valid()) return nullptr;
        if (handle.index >= m_slots.size()) return nullptr;
        const auto& slot = m_slots[handle.index];
        if (!slot.occupied || slot.generation != handle.generation) return nullptr;
        return &slot.data;
    }

    // Fill a previously allocated slot with data.
    void fill(Handle<T> handle, T value) {
        if (!handle.valid()) return;
        CHECK(handle.index < m_slots.size());
        auto& slot = m_slots[handle.index];
        CHECK(slot.generation == handle.generation);
        if (!slot.occupied) {
            slot.occupied = true;
            ++m_activeCount;
        }
        slot.data = std::move(value);
    }

    void incrementRef(Handle<T> handle) {
        if (!handle.valid()) return;
        if (handle.index >= m_refCounts.size()) return;
        auto& slot = m_slots[handle.index];
        if (slot.generation != handle.generation) return;
        m_refCounts[handle.index]++;
    }

    void decrementRef(Handle<T> handle) {
        if (!handle.valid()) return;
        if (handle.index >= m_refCounts.size()) return;
        auto& slot = m_slots[handle.index];
        if (slot.generation != handle.generation) return;
        CHECK(m_refCounts[handle.index] > 0);
        m_refCounts[handle.index]--;
    }

    [[nodiscard]] u32 refCount(Handle<T> handle) const {
        if (!handle.valid()) return 0;
        if (handle.index >= m_refCounts.size()) return 0;
        const auto& slot = m_slots[handle.index];
        if (slot.generation != handle.generation) return 0;
        return m_refCounts[handle.index];
    }

    [[nodiscard]] usize slotCount() const { return m_slots.size(); }
    [[nodiscard]] usize activeCount() const { return m_activeCount; }

    [[nodiscard]] bool isOccupied(Handle<T> handle) const {
        if (!handle.valid()) return false;
        if (handle.index >= m_slots.size()) return false;
        const auto& slot = m_slots[handle.index];
        return slot.occupied && slot.generation == handle.generation;
    }

    void clear() {
        for (usize i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].occupied) {
                std::destroy_at(&m_slots[i].data);
            }
        }
        m_slots.clear();
        m_refCounts.clear();
        m_freeList.clear();
        m_activeCount = 0;
    }

private:
    DynamicArray<HandleTableSlot<T>> m_slots;
    DynamicArray<u32> m_refCounts;
    DynamicArray<u32> m_freeList;
    usize m_activeCount = 0;
};

} // namespace Entelechy
