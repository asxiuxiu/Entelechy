#pragma once
#include "allocator.h"
#include <initializer_list>
#include <memory>
#include <utility>
#include <type_traits>
#include <cstring>

namespace Entelechy {

// Dynamic array with a pluggable allocator.
// AllocatorT must provide static methods:
//   void* alloc(usize size, usize align)
//   void  free(void* ptr)
//
// Allocation decision guide: see ALLOCATOR_GUIDE.md
template <typename T, typename AllocatorT = DefaultAllocator>
class DynamicArray {
public:
    DynamicArray() : m_data(nullptr), m_count(0), m_capacity(0) {}

    DynamicArray(std::initializer_list<T> init) : m_data(nullptr), m_count(0), m_capacity(0) {
        reserve(init.size());
        for (const auto& item : init) {
            pushBack(item);
        }
    }
    ~DynamicArray() { clear(); }

    DynamicArray(const DynamicArray& other) : m_data(nullptr), m_count(0), m_capacity(0) {
        reserve(other.m_count);
        for (usize i = 0; i < other.m_count; ++i) {
            pushBack(other.m_data[i]);
        }
    }

    DynamicArray& operator=(const DynamicArray& other) {
        if (this != &other) {
            clear();
            reserve(other.m_count);
            for (usize i = 0; i < other.m_count; ++i) {
                pushBack(other.m_data[i]);
            }
        }
        return *this;
    }

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
        std::construct_at(&m_data[m_count], value);
        ++m_count;
    }

    void pushBack(T&& value) {
        if (m_count >= m_capacity) {
            grow(m_capacity == 0 ? 4 : m_capacity * 2);
        }
        std::construct_at(&m_data[m_count], std::move(value));
        ++m_count;
    }

    void popBack() {
        if (m_count > 0) {
            --m_count;
            std::destroy_at(&m_data[m_count]);
        }
    }

    void resize(usize newCount) {
        if (newCount > m_capacity) {
            grow(newCount);
        }
        for (usize i = m_count; i < newCount; ++i) {
            std::construct_at(&m_data[i]);
        }
        for (usize i = newCount; i < m_count; ++i) {
            std::destroy_at(&m_data[i]);
        }
        m_count = newCount;
    }

    void resize(usize newCount, const T& value) {
        if (newCount > m_capacity) {
            grow(newCount);
        }
        for (usize i = m_count; i < newCount; ++i) {
            std::construct_at(&m_data[i], value);
        }
        for (usize i = newCount; i < m_count; ++i) {
            std::destroy_at(&m_data[i]);
        }
        m_count = newCount;
    }

    void clear() {
        for (usize i = m_count; i > 0; --i) {
            std::destroy_at(&m_data[i - 1]);
        }
        if (m_data) {
            AllocatorT::free(m_data);
            m_data = nullptr;
        }
        m_count = 0;
        m_capacity = 0;
    }

    void swap(DynamicArray& other) noexcept {
        using std::swap;
        swap(m_data, other.m_data);
        swap(m_count, other.m_count);
        swap(m_capacity, other.m_capacity);
    }

    void reserve(usize newCapacity) {
        if (newCapacity > m_capacity) {
            grow(newCapacity);
        }
    }

    [[nodiscard]] T* data() { return m_data; }
    [[nodiscard]] const T* data() const { return m_data; }
    [[nodiscard]] usize size() const { return m_count; }
    [[nodiscard]] usize capacity() const { return m_capacity; }
    [[nodiscard]] bool empty() const { return m_count == 0; }

    T& operator[](usize i) { return m_data[i]; }
    const T& operator[](usize i) const { return m_data[i]; }

    [[nodiscard]] T& front() { return m_data[0]; }
    [[nodiscard]] const T& front() const { return m_data[0]; }
    [[nodiscard]] T& back() { return m_data[m_count - 1]; }
    [[nodiscard]] const T& back() const { return m_data[m_count - 1]; }

    [[nodiscard]] T* begin() { return m_data; }
    [[nodiscard]] T* end() { return m_data + m_count; }
    [[nodiscard]] const T* begin() const { return m_data; }
    [[nodiscard]] const T* end() const { return m_data + m_count; }

    void insert(usize index, const T& value) {
        CHECK(index <= m_count);
        if (m_count >= m_capacity) {
            grow(m_capacity == 0 ? 4 : m_capacity * 2);
        }
        for (usize i = m_count; i > index; --i) {
            std::construct_at(&m_data[i], std::move(m_data[i - 1]));
            std::destroy_at(&m_data[i - 1]);
        }
        std::construct_at(&m_data[index], value);
        ++m_count;
    }

    void insert(usize index, T&& value) {
        CHECK(index <= m_count);
        if (m_count >= m_capacity) {
            grow(m_capacity == 0 ? 4 : m_capacity * 2);
        }
        for (usize i = m_count; i > index; --i) {
            std::construct_at(&m_data[i], std::move(m_data[i - 1]));
            std::destroy_at(&m_data[i - 1]);
        }
        std::construct_at(&m_data[index], std::move(value));
        ++m_count;
    }

    void removeAt(usize index) {
        CHECK(index < m_count);
        std::destroy_at(&m_data[index]);
        for (usize i = index; i + 1 < m_count; ++i) {
            std::construct_at(&m_data[i], std::move(m_data[i + 1]));
            std::destroy_at(&m_data[i + 1]);
        }
        --m_count;
    }

    [[nodiscard]] usize find(const T& value) const {
        for (usize i = 0; i < m_count; ++i) {
            if (m_data[i] == value) {
                return i;
            }
        }
        return static_cast<usize>(-1);
    }

private:
    void grow(usize newCapacity) {
        T* newData = static_cast<T*>(AllocatorT::alloc(newCapacity * sizeof(T), alignof(T)));
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(newData, m_data, m_count * sizeof(T));
        } else {
            for (usize i = 0; i < m_count; ++i) {
                std::construct_at(&newData[i], std::move(m_data[i]));
                std::destroy_at(&m_data[i]);
            }
        }
        if (m_data) {
            AllocatorT::free(m_data);
        }
        m_data = newData;
        m_capacity = newCapacity;
    }

    T* m_data;
    usize m_count;
    usize m_capacity;
};

} // namespace Entelechy
