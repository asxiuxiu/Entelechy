#pragma once
#include "types.h"
#include "dynamic_array.h"
#include "allocator.h"
#include <cstdint>
#include <cstddef>

namespace Entelechy {

class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    virtual void remove(Entity e) = 0;
    [[nodiscard]] virtual bool has(Entity e) const = 0;
    [[nodiscard]] virtual size_t count() const = 0;
    [[nodiscard]] virtual const void* getRaw(Entity e) const = 0;
    [[nodiscard]] virtual void* getRaw(Entity e) = 0;
    [[nodiscard]] virtual const uint32_t* entityIds() const = 0;
};

template<typename T>
class ComponentArray : public IComponentArray {
public:
    void set(Entity e, const T& value) {
        if (e.id >= m_entityIndex.size()) {
            m_entityIndex.resize(e.id + 1, -1);
        }
        int32_t idx = m_entityIndex[e.id];
        if (idx == -1) {
            idx = static_cast<int32_t>(m_dense.size());
            m_entityIndex[e.id] = idx;
            m_dense.pushBack(value);
            m_denseEntityIds.pushBack(e.id);
        } else {
            m_dense[idx] = value;
        }
    }

    void remove(Entity e) override {
        if (e.id >= m_entityIndex.size() || m_entityIndex[e.id] == -1) return;
        int32_t idx = m_entityIndex[e.id];
        int32_t lastIdx = static_cast<int32_t>(m_dense.size()) - 1;
        // swap-and-pop to keep dense
        m_dense[idx] = m_dense[lastIdx];
        m_denseEntityIds[idx] = m_denseEntityIds[lastIdx];
        m_entityIndex[m_denseEntityIds[lastIdx]] = idx;
        m_dense.popBack();
        m_denseEntityIds.popBack();
        m_entityIndex[e.id] = -1;
    }

    bool has(Entity e) const override {
        return e.id < m_entityIndex.size() && m_entityIndex[e.id] != -1;
    }

    [[nodiscard]] T* get(Entity e) {
        if (!has(e)) return nullptr;
        return &m_dense[m_entityIndex[e.id]];
    }

    [[nodiscard]] const T* get(Entity e) const {
        if (!has(e)) return nullptr;
        return &m_dense[m_entityIndex[e.id]];
    }

    [[nodiscard]] size_t count() const override {
        return m_dense.size();
    }

    [[nodiscard]] T* data() { return m_dense.data(); }
    [[nodiscard]] const T* data() const { return m_dense.data(); }

    [[nodiscard]] const uint32_t* entityIds() const override {
        return m_denseEntityIds.data();
    }

    [[nodiscard]] const void* getRaw(Entity e) const override {
        return get(e);
    }

    [[nodiscard]] void* getRaw(Entity e) override {
        return get(e);
    }

private:
    DynamicArray<int32_t> m_entityIndex;    // sparse table: entity.id -> dense index (or -1)
    DynamicArray<T> m_dense;                // dense component data
    DynamicArray<uint32_t> m_denseEntityIds; // dense[i] corresponds to which entity.id
};

} // namespace Entelechy
