#pragma once
#include "foundation_types.h"
#include "allocator.h"
#include <utility>

namespace Entelechy {

// Small-array optimization: first N elements live inline;
// overflow goes to the heap allocator.
template <typename T, usize N>
class InlineArray {
public:
    InlineArray() : m_count(0), m_heapData(nullptr), m_heapCapacity(0) {}
    ~InlineArray() { clear(); }

    InlineArray(const InlineArray&) = delete;
    InlineArray& operator=(const InlineArray&) = delete;

    InlineArray(InlineArray&& other) noexcept
        : m_count(other.m_count)
        , m_heapData(other.m_heapData)
        , m_heapCapacity(other.m_heapCapacity) {
        for (usize i = 0; i < other.m_count && i < N; ++i) {
            std::construct_at(&inlineSlot(i), std::move(other.inlineSlot(i)));
            std::destroy_at(&other.inlineSlot(i));
        }
        other.m_count = 0;
        other.m_heapData = nullptr;
        other.m_heapCapacity = 0;
    }

    InlineArray& operator=(InlineArray&& other) noexcept {
        if (this != &other) {
            clear();
            for (usize i = 0; i < other.m_count && i < N; ++i) {
                std::construct_at(&inlineSlot(i), std::move(other.inlineSlot(i)));
                std::destroy_at(&other.inlineSlot(i));
            }
            m_count = other.m_count;
            m_heapData = other.m_heapData;
            m_heapCapacity = other.m_heapCapacity;
            other.m_count = 0;
            other.m_heapData = nullptr;
            other.m_heapCapacity = 0;
        }
        return *this;
    }

    void pushBack(const T& value) {
        ensureSpace();
        std::construct_at(&slot(m_count), value);
        ++m_count;
    }

    void pushBack(T&& value) {
        ensureSpace();
        std::construct_at(&slot(m_count), std::move(value));
        ++m_count;
    }

    void popBack() {
        if (m_count > 0) {
            --m_count;
            std::destroy_at(&slot(m_count));
        }
    }

    void clear() {
        for (usize i = m_count; i > 0; --i) {
            std::destroy_at(&slot(i - 1));
        }
        if (m_heapData) {
            DefaultAllocator::free(m_heapData);
            m_heapData = nullptr;
        }
        m_count = 0;
        m_heapCapacity = 0;
    }

    void resize(usize newCount) {
        if (newCount > m_count) {
            ensureSpaceFor(newCount);
            for (usize i = m_count; i < newCount; ++i) {
                std::construct_at(&slot(i));
            }
        } else if (newCount < m_count) {
            for (usize i = newCount; i < m_count; ++i) {
                std::destroy_at(&slot(i));
            }
        }
        m_count = newCount;
    }

    void resize(usize newCount, const T& value) {
        if (newCount > m_count) {
            ensureSpaceFor(newCount);
            for (usize i = m_count; i < newCount; ++i) {
                std::construct_at(&slot(i), value);
            }
        } else if (newCount < m_count) {
            for (usize i = newCount; i < m_count; ++i) {
                std::destroy_at(&slot(i));
            }
        }
        m_count = newCount;
    }

    [[nodiscard]] T* data() { return m_count <= N ? inlinePtr() : m_heapData; }
    [[nodiscard]] const T* data() const { return m_count <= N ? inlinePtr() : m_heapData; }
    [[nodiscard]] usize size() const { return m_count; }
    [[nodiscard]] usize capacity() const { return m_heapCapacity > 0 ? m_heapCapacity : N; }
    [[nodiscard]] bool empty() const { return m_count == 0; }

    T& operator[](usize i) { return slot(i); }
    const T& operator[](usize i) const { return slot(i); }

    [[nodiscard]] T& front() { return slot(0); }
    [[nodiscard]] const T& front() const { return slot(0); }
    [[nodiscard]] T& back() { return slot(m_count - 1); }
    [[nodiscard]] const T& back() const { return slot(m_count - 1); }

    [[nodiscard]] T* begin() { return data(); }
    [[nodiscard]] T* end() { return data() + m_count; }
    [[nodiscard]] const T* begin() const { return data(); }
    [[nodiscard]] const T* end() const { return data() + m_count; }

private:
    alignas(alignof(T)) u8 m_inline[N * sizeof(T)];
    T* m_heapData;
    usize m_count;
    usize m_heapCapacity;

    T* inlinePtr() { return reinterpret_cast<T*>(m_inline); }
    const T* inlinePtr() const { return reinterpret_cast<const T*>(m_inline); }

    T& inlineSlot(usize i) { return reinterpret_cast<T*>(m_inline)[i]; }
    const T& inlineSlot(usize i) const { return reinterpret_cast<const T*>(m_inline)[i]; }

    T& slot(usize i) { return (i < N && m_heapData == nullptr) ? inlineSlot(i) : m_heapData[i]; }
    const T& slot(usize i) const { return (i < N && m_heapData == nullptr) ? inlineSlot(i) : m_heapData[i]; }

    void ensureSpace() {
        if (m_count < N) return;
        if (m_count < m_heapCapacity) return;
        ensureSpaceFor(m_count + 1);
    }

    void ensureSpaceFor(usize needed) {
        if (needed <= N && m_heapData == nullptr) return;
        usize newCap = m_heapCapacity == 0 ? 4 : m_heapCapacity;
        while (newCap < needed) newCap *= 2;
        if (newCap < N) newCap = N;
        T* newHeap = static_cast<T*>(DefaultAllocator::alloc(newCap * sizeof(T), alignof(T)));
        usize heapStart = m_heapData ? 0 : N;
        for (usize i = 0; i < m_count; ++i) {
            std::construct_at(&newHeap[i], std::move(slot(i)));
            std::destroy_at(&slot(i));
        }
        if (m_heapData) {
            DefaultAllocator::free(m_heapData);
        }
        m_heapData = newHeap;
        m_heapCapacity = newCap;
    }
};

} // namespace Entelechy
