#pragma once
#include "foundation_types.h"
#include "allocator.h"
#include <functional>
#include <utility>

namespace Entelechy {

// ------------------------------------------------------------------
// DefaultHash
// ------------------------------------------------------------------
template <typename K>
struct DefaultHash {
    u64 operator()(const K& key) const {
        return static_cast<u64>(std::hash<K>{}(key));
    }
};

// ------------------------------------------------------------------
// HashMap
// ------------------------------------------------------------------
// Open-addressing hash table with linear probing.
// Capacity is always a power of two; index = hash & (capacity - 1).
// No deletion support in Phase B (no tombstones).
template <typename K, typename V, typename Hash = DefaultHash<K>, typename Allocator = DefaultAllocator>
class HashMap {
public:
    struct KeyValueRef {
        const K& first;
        V& second;
        KeyValueRef(const K& k, V& v) : first(k), second(v) {}
    };

    struct ConstKeyValueRef {
        const K& first;
        const V& second;
        ConstKeyValueRef(const K& k, const V& v) : first(k), second(v) {}
    };

    HashMap() : m_entries(nullptr), m_capacity(0), m_count(0) {}

    ~HashMap() {
        if (m_entries) {
            for (usize i = 0; i < m_capacity; ++i) {
                std::destroy_at(&m_entries[i]);
            }
            Allocator::free(m_entries);
        }
    }

    HashMap(const HashMap&) = delete;
    HashMap& operator=(const HashMap&) = delete;

    HashMap(HashMap&& other) noexcept
        : m_entries(other.m_entries)
        , m_capacity(other.m_capacity)
        , m_count(other.m_count) {
        other.m_entries = nullptr;
        other.m_capacity = 0;
        other.m_count = 0;
    }

    HashMap& operator=(HashMap&& other) noexcept {
        if (this != &other) {
            if (m_entries) {
                for (usize i = 0; i < m_capacity; ++i) {
                    std::destroy_at(&m_entries[i]);
                }
                Allocator::free(m_entries);
            }
            m_entries = other.m_entries;
            m_capacity = other.m_capacity;
            m_count = other.m_count;
            other.m_entries = nullptr;
            other.m_capacity = 0;
            other.m_count = 0;
        }
        return *this;
    }

    void insert(const K& key, const V& value) {
        if (m_count * 2 >= m_capacity) {
            grow();
        }
        u64 h = Hash{}(key);
        usize idx = h & (m_capacity - 1);
        while (m_entries[idx].occupied) {
            if (m_entries[idx].hash == h && m_entries[idx].key == key) {
                m_entries[idx].value = value;
                return;
            }
            idx = (idx + 1) & (m_capacity - 1);
        }
        std::construct_at(&m_entries[idx].key, key);
        std::construct_at(&m_entries[idx].value, value);
        m_entries[idx].hash = h;
        m_entries[idx].occupied = true;
        ++m_count;
    }

    void insert(const K& key, V&& value) {
        if (m_count * 2 >= m_capacity) {
            grow();
        }
        u64 h = Hash{}(key);
        usize idx = h & (m_capacity - 1);
        while (m_entries[idx].occupied) {
            if (m_entries[idx].hash == h && m_entries[idx].key == key) {
                m_entries[idx].value = std::move(value);
                return;
            }
            idx = (idx + 1) & (m_capacity - 1);
        }
        std::construct_at(&m_entries[idx].key, key);
        std::construct_at(&m_entries[idx].value, std::move(value));
        m_entries[idx].hash = h;
        m_entries[idx].occupied = true;
        ++m_count;
    }

    V* find(const K& key) {
        if (m_count == 0) return nullptr;
        u64 h = Hash{}(key);
        usize idx = h & (m_capacity - 1);
        while (m_entries[idx].occupied) {
            if (m_entries[idx].hash == h && m_entries[idx].key == key) {
                return &m_entries[idx].value;
            }
            idx = (idx + 1) & (m_capacity - 1);
        }
        return nullptr;
    }

    const V* find(const K& key) const {
        if (m_count == 0) return nullptr;
        u64 h = Hash{}(key);
        usize idx = h & (m_capacity - 1);
        while (m_entries[idx].occupied) {
            if (m_entries[idx].hash == h && m_entries[idx].key == key) {
                return &m_entries[idx].value;
            }
            idx = (idx + 1) & (m_capacity - 1);
        }
        return nullptr;
    }

    bool contains(const K& key) const {
        return find(key) != nullptr;
    }

    V& operator[](const K& key) {
        if (auto* v = find(key)) return *v;
        insert(key, V{});
        return *find(key);
    }

    [[nodiscard]] usize size() const { return m_count; }
    [[nodiscard]] bool empty() const { return m_count == 0; }

    void clear() {
        if (m_entries) {
            for (usize i = 0; i < m_capacity; ++i) {
                if (m_entries[i].occupied) {
                    std::destroy_at(&m_entries[i].key);
                    std::destroy_at(&m_entries[i].value);
                    m_entries[i].occupied = false;
                }
            }
        }
        m_count = 0;
    }

    // Iteration -------------------------------------------------------
    class Iterator {
    public:
        Iterator(HashMap* map, usize idx) : m_map(map), m_idx(idx) {}

        KeyValueRef operator*() const {
            return KeyValueRef{m_map->m_entries[m_idx].key, m_map->m_entries[m_idx].value};
        }

        bool operator!=(const Iterator& other) const { return m_idx != other.m_idx; }
        bool operator==(const Iterator& other) const { return m_idx == other.m_idx; }

        Iterator& operator++() {
            ++m_idx;
            skipEmpty();
            return *this;
        }

    private:
        void skipEmpty() {
            while (m_idx < m_map->m_capacity && !m_map->m_entries[m_idx].occupied) {
                ++m_idx;
            }
        }

        HashMap* m_map;
        usize m_idx;
    };

    class ConstIterator {
    public:
        ConstIterator(const HashMap* map, usize idx) : m_map(map), m_idx(idx) {}

        ConstKeyValueRef operator*() const {
            return ConstKeyValueRef{m_map->m_entries[m_idx].key, m_map->m_entries[m_idx].value};
        }

        bool operator!=(const ConstIterator& other) const { return m_idx != other.m_idx; }
        bool operator==(const ConstIterator& other) const { return m_idx == other.m_idx; }

        ConstIterator& operator++() {
            ++m_idx;
            skipEmpty();
            return *this;
        }

    private:
        void skipEmpty() {
            while (m_idx < m_map->m_capacity && !m_map->m_entries[m_idx].occupied) {
                ++m_idx;
            }
        }

        const HashMap* m_map;
        usize m_idx;
    };

    Iterator begin() {
        if (!m_entries || m_count == 0) return end();
        usize idx = 0;
        while (idx < m_capacity && !m_entries[idx].occupied) ++idx;
        return Iterator(this, idx);
    }

    Iterator end() {
        return Iterator(this, m_capacity);
    }

    ConstIterator begin() const {
        if (!m_entries || m_count == 0) return end();
        usize idx = 0;
        while (idx < m_capacity && !m_entries[idx].occupied) ++idx;
        return ConstIterator(this, idx);
    }

    ConstIterator end() const {
        return ConstIterator(this, m_capacity);
    }

private:
    struct Entry {
        u64 hash = 0;
        K key;
        V value;
        bool occupied = false;
    };

    void grow() {
        usize oldCap = m_capacity;
        usize newCap = oldCap == 0 ? 16 : oldCap * 2;
        Entry* oldEntries = m_entries;

        m_entries = static_cast<Entry*>(Allocator::alloc(newCap * sizeof(Entry), alignof(Entry)));
        for (usize i = 0; i < newCap; ++i) {
            std::construct_at(&m_entries[i]);
            m_entries[i].occupied = false;
        }
        m_capacity = newCap;

        if (oldEntries) {
            for (usize i = 0; i < oldCap; ++i) {
                if (oldEntries[i].occupied) {
                    u64 h = oldEntries[i].hash;
                    usize idx = h & (newCap - 1);
                    while (m_entries[idx].occupied) {
                        idx = (idx + 1) & (newCap - 1);
                    }
                    std::construct_at(&m_entries[idx].key, std::move(oldEntries[i].key));
                    std::construct_at(&m_entries[idx].value, std::move(oldEntries[i].value));
                    m_entries[idx].hash = h;
                    m_entries[idx].occupied = true;

                    std::destroy_at(&oldEntries[i]);
                } else {
                    std::destroy_at(&oldEntries[i]);
                }
            }
            Allocator::free(oldEntries);
        }
    }

    Entry* m_entries;
    usize m_capacity;
    usize m_count;
};

// ------------------------------------------------------------------
// HashSet
// ------------------------------------------------------------------
template <typename K, typename Hash = DefaultHash<K>, typename Allocator = DefaultAllocator>
class HashSet {
public:
    void insert(const K& key) { m_map.insert(key, true); }
    bool contains(const K& key) const { return m_map.contains(key); }
    void clear() { m_map.clear(); }
    [[nodiscard]] usize size() const { return m_map.size(); }
    [[nodiscard]] bool empty() const { return m_map.empty(); }

    class Iterator {
    public:
        Iterator(typename HashMap<K, bool, Hash, Allocator>::Iterator it) : m_it(it) {}
        const K& operator*() const { return (*m_it).first; }
        bool operator!=(const Iterator& other) const { return m_it != other.m_it; }
        bool operator==(const Iterator& other) const { return m_it == other.m_it; }
        Iterator& operator++() { ++m_it; return *this; }
    private:
        typename HashMap<K, bool, Hash, Allocator>::Iterator m_it;
    };

    class ConstIterator {
    public:
        ConstIterator(typename HashMap<K, bool, Hash, Allocator>::ConstIterator it) : m_it(it) {}
        const K& operator*() const { return (*m_it).first; }
        bool operator!=(const ConstIterator& other) const { return m_it != other.m_it; }
        bool operator==(const ConstIterator& other) const { return m_it == other.m_it; }
        ConstIterator& operator++() { ++m_it; return *this; }
    private:
        typename HashMap<K, bool, Hash, Allocator>::ConstIterator m_it;
    };

    Iterator begin() { return Iterator(m_map.begin()); }
    Iterator end() { return Iterator(m_map.end()); }

    ConstIterator begin() const { return ConstIterator(m_map.begin()); }
    ConstIterator end() const { return ConstIterator(m_map.end()); }

private:
    HashMap<K, bool, Hash, Allocator> m_map;
};

} // namespace Entelechy
