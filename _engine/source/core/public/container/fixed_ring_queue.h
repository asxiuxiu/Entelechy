#pragma once
#include "core/foundation_types.h"
#include <memory>
#include <utility>

namespace Entelechy
{

// ============================================================
// Fixed-capacity ring queue (circular buffer)
// ============================================================
// Stores elements in a fixed-size ring buffer. When full, pushBack()
// overwrites the oldest element (like a sliding window).
//
// Unlike RingBuffer (SPSC lock-free), this is NOT thread-safe and is
// designed for single-threaded scenarios requiring O(1) push/pop at
// both ends and cache-friendly iteration.
//
// Typical use cases:
// - Logger history (last N entries)
// - Fixed-size event windows
// - Rolling statistics buffers
//
// Reference: UE's TCircularBuffer, but simplified for our allocator model.
template <typename T, usize Capacity>
class FixedRingQueue
{
public:
    FixedRingQueue() : m_head(0), m_count(0) {}
    ~FixedRingQueue()
    {
        clear();
    }

    FixedRingQueue(const FixedRingQueue &) = delete;
    FixedRingQueue &operator=(const FixedRingQueue &) = delete;

    FixedRingQueue(FixedRingQueue &&other) noexcept : m_head(0), m_count(other.m_count)
    {
        for (usize i = 0; i < other.m_count; ++i)
        {
            std::construct_at(ptr(i), std::move(other[i]));
            std::destroy_at(other.ptr((other.m_head + i) % Capacity));
        }
        other.m_head = 0;
        other.m_count = 0;
    }

    FixedRingQueue &operator=(FixedRingQueue &&other) noexcept
    {
        if (this != &other)
        {
            clear();
            m_head = 0;
            m_count = other.m_count;
            for (usize i = 0; i < other.m_count; ++i)
            {
                std::construct_at(ptr(i), std::move(other[i]));
                std::destroy_at(other.ptr((other.m_head + i) % Capacity));
            }
            other.m_head = 0;
            other.m_count = 0;
        }
        return *this;
    }

    void pushBack(const T &value)
    {
        if (m_count < Capacity)
        {
            std::construct_at(ptr((m_head + m_count) % Capacity), value);
            ++m_count;
        }
        else
        {
            std::destroy_at(ptr(m_head));
            std::construct_at(ptr(m_head), value);
            m_head = (m_head + 1) % Capacity;
        }
    }

    void pushBack(T &&value)
    {
        if (m_count < Capacity)
        {
            std::construct_at(ptr((m_head + m_count) % Capacity), std::move(value));
            ++m_count;
        }
        else
        {
            std::destroy_at(ptr(m_head));
            std::construct_at(ptr(m_head), std::move(value));
            m_head = (m_head + 1) % Capacity;
        }
    }

    void popFront()
    {
        if (m_count > 0)
        {
            std::destroy_at(ptr(m_head));
            m_head = (m_head + 1) % Capacity;
            --m_count;
        }
    }

    [[nodiscard]] T &front()
    {
        return *ptr(m_head);
    }
    [[nodiscard]] const T &front() const
    {
        return *ptr(m_head);
    }

    [[nodiscard]] T &back()
    {
        return *ptr((m_head + m_count - 1) % Capacity);
    }
    [[nodiscard]] const T &back() const
    {
        return *ptr((m_head + m_count - 1) % Capacity);
    }

    [[nodiscard]] T &operator[](usize i)
    {
        return *ptr((m_head + i) % Capacity);
    }
    [[nodiscard]] const T &operator[](usize i) const
    {
        return *ptr((m_head + i) % Capacity);
    }

    [[nodiscard]] usize size() const
    {
        return m_count;
    }
    [[nodiscard]] static constexpr usize capacity()
    {
        return Capacity;
    }
    [[nodiscard]] bool empty() const
    {
        return m_count == 0;
    }
    [[nodiscard]] bool full() const
    {
        return m_count == Capacity;
    }

    void clear()
    {
        for (usize i = 0; i < m_count; ++i)
        {
            std::destroy_at(ptr((m_head + i) % Capacity));
        }
        m_head = 0;
        m_count = 0;
    }

    class Iterator
    {
    public:
        Iterator(FixedRingQueue *queue, usize idx) : m_queue(queue), m_idx(idx) {}
        T &operator*() const
        {
            return (*m_queue)[m_idx];
        }
        T *operator->() const
        {
            return &(*m_queue)[m_idx];
        }
        Iterator &operator++()
        {
            ++m_idx;
            return *this;
        }
        bool operator!=(const Iterator &other) const
        {
            return m_idx != other.m_idx;
        }
        bool operator==(const Iterator &other) const
        {
            return m_idx == other.m_idx;
        }

    private:
        FixedRingQueue *m_queue;
        usize m_idx;
    };

    class ConstIterator
    {
    public:
        ConstIterator(const FixedRingQueue *queue, usize idx) : m_queue(queue), m_idx(idx) {}
        const T &operator*() const
        {
            return (*m_queue)[m_idx];
        }
        const T *operator->() const
        {
            return &(*m_queue)[m_idx];
        }
        ConstIterator &operator++()
        {
            ++m_idx;
            return *this;
        }
        bool operator!=(const ConstIterator &other) const
        {
            return m_idx != other.m_idx;
        }
        bool operator==(const ConstIterator &other) const
        {
            return m_idx == other.m_idx;
        }

    private:
        const FixedRingQueue *m_queue;
        usize m_idx;
    };

    [[nodiscard]] Iterator begin()
    {
        return Iterator(this, 0);
    }
    [[nodiscard]] Iterator end()
    {
        return Iterator(this, m_count);
    }
    [[nodiscard]] ConstIterator begin() const
    {
        return ConstIterator(this, 0);
    }
    [[nodiscard]] ConstIterator end() const
    {
        return ConstIterator(this, m_count);
    }

private:
    T *ptr(usize i)
    {
        return reinterpret_cast<T *>(m_storage) + i;
    }
    const T *ptr(usize i) const
    {
        return reinterpret_cast<const T *>(m_storage) + i;
    }

    alignas(T) char m_storage[Capacity * sizeof(T)];
    usize m_head;
    usize m_count;
};

} // namespace Entelechy
