#pragma once
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <utility>

namespace Entelechy {

// Fixed-size object pool: pre-allocates a large block of equally-sized Slots.
// Unused Slots are managed via a free list. Both alloc and free are O(1).
template <typename T, size_t BLOCK_COUNT>
class ObjectPool {
public:
    union Slot {
        T object;
        Slot* nextFree;
        Slot() {}
        ~Slot() {}
    };

    ObjectPool();
    ~ObjectPool();

    template <typename... Args>
    [[nodiscard]] T* allocate(Args&&... args);
    void free(T* obj);

    [[nodiscard]] bool empty() const { return m_free_list == nullptr; }

private:
    Slot* m_memory;
    Slot* m_free_list;
};

template <typename T, size_t BLOCK_COUNT>
ObjectPool<T, BLOCK_COUNT>::ObjectPool()
    : m_memory(static_cast<Slot*>(std::malloc(sizeof(Slot) * BLOCK_COUNT)))
    , m_free_list(nullptr) {
    for (size_t i = 0; i < BLOCK_COUNT; ++i) {
        m_memory[i].nextFree = (i + 1 < BLOCK_COUNT) ? &m_memory[i + 1] : nullptr;
    }
    m_free_list = &m_memory[0];
}

template <typename T, size_t BLOCK_COUNT>
ObjectPool<T, BLOCK_COUNT>::~ObjectPool() {
    std::free(m_memory);
}

template <typename T, size_t BLOCK_COUNT>
template <typename... Args>
T* ObjectPool<T, BLOCK_COUNT>::allocate(Args&&... args) {
    if (!m_free_list) return nullptr;
    Slot* slot = m_free_list;
    m_free_list = m_free_list->nextFree;
    return std::construct_at(slot, std::forward<Args>(args)...);
}

template <typename T, size_t BLOCK_COUNT>
void ObjectPool<T, BLOCK_COUNT>::free(T* obj) {
    if (!obj) return;
    Slot* slot = reinterpret_cast<Slot*>(obj);
    std::destroy_at(&slot->object);
    slot->nextFree = m_free_list;
    m_free_list = slot;
}

} // namespace Entelechy
