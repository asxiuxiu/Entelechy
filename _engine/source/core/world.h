#pragma once
#include "types.h"
#include "component_array.h"
#include "type_registry.h"
#include "dynamic_array.h"
#include <cstdio>
#include <unordered_map>
#include <memory>
#include <tuple>

namespace Entelechy {

class World {
public:
    [[nodiscard]] Entity spawn() {
        Entity e;
        if (!m_freeEntities.empty()) {
            e.id = m_freeEntities.back();
            m_freeEntities.popBack();
            e.generation = m_entityGenerations[e.id];
        } else {
            e.id = static_cast<u32>(m_entityGenerations.size());
            m_entityGenerations.pushBack(0);
            m_entityMasks.pushBack(0);
        }
        m_entityMasks[e.id] = 0;
        return e;
    }

    void destroy(Entity e) {
        if (!valid(e)) return;
        // remove all components
        for (auto& pair : m_componentArrays) {
            pair.second->remove(e);
        }
        m_entityMasks[e.id] = 0;
        m_entityGenerations[e.id]++;
        m_freeEntities.pushBack(e.id);
    }

    [[nodiscard]] bool valid(Entity e) const {
        return e.id < m_entityGenerations.size() && m_entityGenerations[e.id] == e.generation;
    }

    [[nodiscard]] usize entityCount() const {
        return m_entityGenerations.size() - m_freeEntities.size();
    }

    [[nodiscard]] usize maxEntityID() const {
        return m_entityGenerations.size();
    }

    [[nodiscard]] u32 getEntityGeneration(u32 id) const {
        if (id < m_entityGenerations.size()) return m_entityGenerations[id];
        return 0xFFFFFFFFu;
    }

    [[nodiscard]] u32 getEntityMask(u32 id) const {
        if (id < m_entityMasks.size()) return m_entityMasks[id];
        return 0;
    }

    // Generic component API
    template<typename T>
    T* addComponent(Entity e, const T& comp) {
        auto* array = getOrCreateComponentArray<T>();
        array->set(e, comp);
        m_entityMasks[e.id] |= Entelechy::TypeRegistry::instance().getMask<T>();
        return array->get(e);
    }

    template<typename T>
    void removeComponent(Entity e) {
        auto* array = getComponentArray<T>();
        if (array) {
            array->remove(e);
            m_entityMasks[e.id] &= ~Entelechy::TypeRegistry::instance().getMask<T>();
        }
    }

    template<typename T>
    [[nodiscard]] T* getComponent(Entity e) {
        auto* array = getComponentArray<T>();
        if (!array) return nullptr;
        return array->get(e);
    }

    template<typename T>
    [[nodiscard]] const T* getComponent(Entity e) const {
        auto* array = getComponentArray<T>();
        if (!array) return nullptr;
        return array->get(e);
    }

    template<typename T>
    [[nodiscard]] bool hasComponent(Entity e) const {
        auto* array = getComponentArray<T>();
        if (!array) return false;
        return array->has(e);
    }

    template<typename T>
    [[nodiscard]] ComponentArray<T>* getComponentArray() {
        Entelechy::ComponentTypeID typeID = Entelechy::TypeRegistry::instance().getTypeID<T>();
        auto it = m_componentArrays.find(typeID);
        if (it == m_componentArrays.end()) return nullptr;
        return static_cast<ComponentArray<T>*>(it->second.get());
    }

    template<typename T>
    [[nodiscard]] const ComponentArray<T>* getComponentArray() const {
        Entelechy::ComponentTypeID typeID = Entelechy::TypeRegistry::instance().getTypeID<T>();
        auto it = m_componentArrays.find(typeID);
        if (it == m_componentArrays.end()) return nullptr;
        return static_cast<const ComponentArray<T>*>(it->second.get());
    }

    const std::unordered_map<Entelechy::ComponentTypeID, std::shared_ptr<IComponentArray>>& componentArrays() const {
        return m_componentArrays;
    }

private:
    template<typename T>
    ComponentArray<T>* getOrCreateComponentArray() {
        Entelechy::ComponentTypeID typeID = Entelechy::TypeRegistry::instance().getTypeID<T>();
        auto it = m_componentArrays.find(typeID);
        if (it != m_componentArrays.end()) {
            return static_cast<ComponentArray<T>*>(it->second.get());
        }
        auto array = std::make_shared<ComponentArray<T>>();
        m_componentArrays[typeID] = array;
        return array.get();
    }

    DynamicArray<u32> m_entityGenerations;
    DynamicArray<u32> m_freeEntities;
    DynamicArray<u32> m_entityMasks;
    std::unordered_map<Entelechy::ComponentTypeID, std::shared_ptr<IComponentArray>> m_componentArrays;
};

} // namespace Entelechy
