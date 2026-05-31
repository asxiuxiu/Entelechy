#pragma once
#include "core/foundation_types.h"
#include <atomic>
#include <utility>

namespace Entelechy {

// Single-producer single-consumer (SPSC) lock-free ring buffer.
// SIZE must be a power of two. Effective capacity is SIZE - 1
// because one slot is reserved to distinguish empty from full.
template <typename T, usize SIZE>
class RingBuffer {
    static_assert((SIZE & (SIZE - 1)) == 0, "RingBuffer SIZE must be a power of two");

public:
    RingBuffer() {
        for (usize i = 0; i < SIZE; ++i) {
            std::construct_at(&slot(i));
        }
    }

    ~RingBuffer() {
        // Destroy any remaining elements
        while (!empty()) {
            T val;
            tryPop(val);
        }
        for (usize i = 0; i < SIZE; ++i) {
            std::destroy_at(&slot(i));
        }
    }

    // Non-copyable, non-movable
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    bool tryPush(const T& value) {
        usize tail = m_tail.load(std::memory_order_relaxed);
        usize next = (tail + 1) & MASK;
        if (next == m_head.load(std::memory_order_acquire)) {
            return false; // full
        }
        slot(tail) = value;
        m_tail.store(next, std::memory_order_release);
        return true;
    }

    bool tryPush(T&& value) {
        usize tail = m_tail.load(std::memory_order_relaxed);
        usize next = (tail + 1) & MASK;
        if (next == m_head.load(std::memory_order_acquire)) {
            return false; // full
        }
        slot(tail) = std::move(value);
        m_tail.store(next, std::memory_order_release);
        return true;
    }

    bool tryPop(T& out) {
        usize head = m_head.load(std::memory_order_relaxed);
        if (head == m_tail.load(std::memory_order_acquire)) {
            return false; // empty
        }
        out = std::move(slot(head));
        std::destroy_at(&slot(head));
        std::construct_at(&slot(head)); // reconstruct for next use
        m_head.store((head + 1) & MASK, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const {
        return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool full() const {
        usize tail = m_tail.load(std::memory_order_acquire);
        usize next = (tail + 1) & MASK;
        return next == m_head.load(std::memory_order_acquire);
    }

    [[nodiscard]] usize size() const {
        usize head = m_head.load(std::memory_order_acquire);
        usize tail = m_tail.load(std::memory_order_acquire);
        return (tail - head) & MASK;
    }

    [[nodiscard]] static constexpr usize capacity() { return SIZE - 1; }

private:
    alignas(alignof(T)) u8 m_data[SIZE * sizeof(T)];
    std::atomic<usize> m_head{0};
    std::atomic<usize> m_tail{0};

    T& slot(usize index) {
        return reinterpret_cast<T*>(m_data)[index & MASK];
    }

    const T& slot(usize index) const {
        return reinterpret_cast<const T*>(m_data)[index & MASK];
    }

    static constexpr usize MASK = SIZE - 1;
};

} // namespace Entelechy
