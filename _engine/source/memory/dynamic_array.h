#pragma once
#include "allocator.h"
#include <cstddef>
#include <utility>

namespace Entelechy {

// Dynamic array with a pluggable allocator
// AllocatorT must provide static methods:
//   void* alloc(size_t size, size_t align)
//   void  free(void* ptr)
template <typename T, typename AllocatorT = DefaultAllocator>
class DynamicArray {
public:
    DynamicArray() : m_data(nullptr), m_count(0), m_capacity(0) {}
    ~DynamicArray() { clear(); }

    // Copy is disallowed; move is allowed
    DynamicArray(const DynamicArray&) = delete;
    DynamicArray& operator=(const DynamicArray&) = delete;

    DynamicArray(DynamicArray&& other) noexcept
        : m_data(other.m_data)
        , m_count(other.m_count)
        , m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_count = 0;
        other.m_capacity = 0;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this != &other) {
            clear();
            m_data = other.m_data;
            m_count = other.m_count;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_count = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    void pushBack(const T& value) {
        if (m_count >= m_capacity) {
            grow(m_capacity == 0 ? 4 : m_capacity * 2);
        }
        new (&m_data[m_count]) T(value);
        ++m_count;
    }

    void pushBack(T&& value) {
        if (m_count >= m_capacity) {
            grow(m_capacity == 0 ? 4 : m_capacity * 2);
        }
        new (&m_data[m_count]) T(std::move(value));
        ++m_count;
    }

    void popBack() {
        if (m_count > 0) {
            --m_count;
            m_data[m_count].~T();
        }
    }

    void resize(size_t newCount) {
        if (newCount > m_capacity) {
            grow(newCount);
        }
        for (size_t i = m_count; i < newCount; ++i) {
            new (&m_data[i]) T();
        }
        for (size_t i = newCount; i < m_count; ++i) {
            m_data[i].~T();
        }
        m_count = newCount;
    }

    void resize(size_t newCount, const T& value) {
        if (newCount > m_capacity) {
            grow(newCount);
        }
        for (size_t i = m_count; i < newCount; ++i) {
            new (&m_data[i]) T(value);
        }
        for (size_t i = newCount; i < m_count; ++i) {
            m_data[i].~T();
        }
        m_count = newCount;
    }

    void clear() {
        for (size_t i = m_count; i > 0; --i) {
            m_data[i - 1].~T();
        }
        if (m_data) {
            AllocatorT::free(m_data);
            m_data = nullptr;
        }
        m_count = 0;
        m_capacity = 0;
    }

    void reserve(size_t newCapacity) {
        if (newCapacity > m_capacity) {
            grow(newCapacity);
        }
    }

    T* data() { return m_data; }
    const T* data() const { return m_data; }
    size_t size() const { return m_count; }
    size_t capacity() const { return m_capacity; }
    bool empty() const { return m_count == 0; }

    T& operator[](size_t i) { return m_data[i]; }
    const T& operator[](size_t i) const { return m_data[i]; }

    T& front() { return m_data[0]; }
    const T& front() const { return m_data[0]; }
    T& back() { return m_data[m_count - 1]; }
    const T& back() const { return m_data[m_count - 1]; }

    T* begin() { return m_data; }
    T* end() { return m_data + m_count; }
    const T* begin() const { return m_data; }
    const T* end() const { return m_data + m_count; }

private:
    void grow(size_t newCapacity) {
        T* newData = static_cast<T*>(AllocatorT::alloc(newCapacity * sizeof(T), alignof(T)));
        for (size_t i = 0; i < m_count; ++i) {
            new (&newData[i]) T(std::move(m_data[i]));
            m_data[i].~T();
        }
        if (m_data) {
            AllocatorT::free(m_data);
        }
        m_data = newData;
        m_capacity = newCapacity;
    }

    T* m_data;
    size_t m_count;
    size_t m_capacity;
};

} // namespace Entelechy
