#pragma once
#include "types.h"
#include "sparse_set.h"
#include "allocator.h"
#include <memory>
#include <utility>

namespace Entelechy {

struct ComponentHooks {
    void(*onRemove)(Entity e, void* component) = nullptr;
    void(*onDrop)(void* component) = nullptr;
};

class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    virtual void remove(Entity e) = 0;
    virtual void setRaw(Entity e, const void* data) = 0;
    [[nodiscard]] virtual bool has(Entity e) const = 0;
    [[nodiscard]] virtual usize count() const = 0;
    [[nodiscard]] virtual const void* getRaw(Entity e) const = 0;
    [[nodiscard]] virtual void* getRaw(Entity e) = 0;
    [[nodiscard]] virtual const u32* entityIds() const = 0;
    [[nodiscard]] virtual ComponentHooks* getHooks() { return nullptr; }
};

// SIMD-aligned column of component data.
// Dense array layout: data[i] corresponds to sparseSet.denseAt(i).
template <typename T>
class Column {
public:
    Column() : m_data(nullptr), m_count(0), m_capacity(0) {}
    ~Column() { clear(); }

    Column(const Column&) = delete;
    Column& operator=(const Column&) = delete;

    Column(Column&& other) noexcept
        : m_data(other.m_data)
        , m_count(other.m_count)
        , m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_count = 0;
        other.m_capacity = 0;
    }

    Column& operator=(Column&& other) noexcept {
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
        if (m_count >= m_capacity) grow();
        std::construct_at(&m_data[m_count], value);
        ++m_count;
    }

    void pushBack(T&& value) {
        if (m_count >= m_capacity) grow();
        std::construct_at(&m_data[m_count], std::move(value));
        ++m_count;
    }

    void swapAndPop(u32 denseIndex) {
        u32 lastIdx = static_cast<u32>(m_count) - 1;
        if (denseIndex != lastIdx) {
            m_data[denseIndex] = std::move(m_data[lastIdx]);
        }
        std::destroy_at(&m_data[lastIdx]);
        --m_count;
    }

    void clear() {
        for (usize i = m_count; i > 0; --i) {
            std::destroy_at(&m_data[i - 1]);
        }
        if (m_data) {
            DefaultAllocator::free(m_data);
            m_data = nullptr;
        }
        m_count = 0;
        m_capacity = 0;
    }

    [[nodiscard]] T* data() { return m_data; }
    [[nodiscard]] const T* data() const { return m_data; }
    [[nodiscard]] usize count() const { return m_count; }
    [[nodiscard]] usize capacity() const { return m_capacity; }

    T& operator[](usize i) { return m_data[i]; }
    const T& operator[](usize i) const { return m_data[i]; }

private:
    void grow() {
        usize newCap = m_capacity == 0 ? 4 : m_capacity * 2;
        constexpr usize ALIGN = (alignof(T) > 16) ? alignof(T) : 16;
        T* newData = static_cast<T*>(DefaultAllocator::alloc(newCap * sizeof(T), ALIGN));
        for (usize i = 0; i < m_count; ++i) {
            std::construct_at(&newData[i], std::move(m_data[i]));
            std::destroy_at(&m_data[i]);
        }
        if (m_data) {
            DefaultAllocator::free(m_data);
        }
        m_data = newData;
        m_capacity = newCap;
    }

    T* m_data;
    usize m_count;
    usize m_capacity;
};

// ComponentArray backed by SparseSet + Column.
// TODO(Phase 4.1): evaluate migration to Archetype Chunk storage for better SoA/cache.
template <typename T>
class ComponentArray : public IComponentArray {
public:
    void set(Entity e, const T& value) {
        if (!m_sparse_set.has(e.id)) {
            m_sparse_set.add(e.id);
            m_column.pushBack(value);
        } else {
            u32 idx = m_sparse_set.indexOf(e.id);
            m_column[idx] = value;
        }
    }

    void setRaw(Entity e, const void* data) override {
        set(e, *static_cast<const T*>(data));
    }

    void remove(Entity e) override {
        if (!m_sparse_set.has(e.id)) return;
        if (m_hooks && m_hooks->onRemove) {
            u32 idx = m_sparse_set.indexOf(e.id);
            m_hooks->onRemove(e, &m_column[idx]);
        }
        u32 idx = m_sparse_set.indexOf(e.id);
        m_column.swapAndPop(idx);
        m_sparse_set.remove(e.id);
    }

    bool has(Entity e) const override {
        return m_sparse_set.has(e.id);
    }

    [[nodiscard]] T* get(Entity e) {
        if (!has(e)) return nullptr;
        return &m_column[m_sparse_set.indexOf(e.id)];
    }

    [[nodiscard]] const T* get(Entity e) const {
        if (!has(e)) return nullptr;
        return &m_column[m_sparse_set.indexOf(e.id)];
    }

    [[nodiscard]] usize count() const override {
        return m_sparse_set.count();
    }

    [[nodiscard]] T* data() { return m_column.data(); }
    [[nodiscard]] const T* data() const { return m_column.data(); }

    [[nodiscard]] const u32* entityIds() const override {
        return m_sparse_set.denseData();
    }

    [[nodiscard]] const void* getRaw(Entity e) const override {
        return get(e);
    }

    [[nodiscard]] void* getRaw(Entity e) override {
        return get(e);
    }

    [[nodiscard]] ComponentHooks* getHooks() override { return m_hooks; }
    void setHooks(ComponentHooks* hooks) { m_hooks = hooks; }

private:
    SparseSet m_sparse_set;
    Column<T> m_column;
    ComponentHooks* m_hooks = nullptr;
};

} // namespace Entelechy
