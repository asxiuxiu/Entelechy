#pragma once
#include "entity_registry.h"
#include "component_array.h"
#include "type_registry.h"
#include "dynamic_array.h"
#include "hash_map.h"
#include <cstdio>
#include <tuple>

namespace Entelechy {

class CommandBuffer;

class World {
public:
    World();
    explicit World(EntityRegistry& registry);
    ~World();

    void bindCommandBuffer(CommandBuffer* cmdBuffer) { m_cmdBuffer = cmdBuffer; }

    [[nodiscard]] Entity spawn() {
        CHECK(m_registry != nullptr);
        Entity e = m_registry->create();
        if (e.id >= m_entityMasks.size()) {
            m_entityMasks.resize(e.id + 1, 0);
        }
        if (e.id >= m_entityRanks.size()) {
            m_entityRanks.resize(e.id + 1, 0);
        }
        m_entityMasks[e.id] = 0;
        m_entityRanks[e.id] = 0;
        return e;
    }

    [[nodiscard]] DynamicArray<Entity> spawnBatch(usize count);

    void destroy(Entity e);
    void destroyImmediate(Entity e);

    [[nodiscard]] bool valid(Entity e) const {
        return m_registry && m_registry->isAlive(e);
    }

    [[nodiscard]] usize entityCount() const {
        return m_registry ? m_registry->aliveCount() : 0;
    }

    [[nodiscard]] usize maxEntityID() const {
        return m_registry ? m_registry->capacity() : 0;
    }

    [[nodiscard]] u32 getEntityGeneration(u32 id) const {
        return m_registry ? m_registry->getGeneration(id) : 0xFFFFFFFFu;
    }

    [[nodiscard]] u32 getEntityMask(u32 id) const {
        if (id < m_entityMasks.size()) return m_entityMasks[id];
        return 0;
    }

    void setCurrentFrame(u64 frame) { m_currentFrame = frame; }
    [[nodiscard]] u64 currentFrame() const { return m_currentFrame; }

    void setParent(Entity child, Entity parent);
    void setParentImmediate(Entity child, Entity parent);

    // KeepWorld semantics: preserves the child's world transform when re-parenting.
    // The child's local transform is recomputed so that its global position
    // remains unchanged after the hierarchy change.
    void setParentInPlace(Entity child, Entity parent);

    [[nodiscard]] u32 getRank(Entity e) const;
    void setRank(Entity e, u32 rank);

    template<typename T>
    T* addComponent(Entity e, const T& comp) {
        auto* array = getOrCreateComponentArray<T>();
        array->set(e, comp);
        m_entityMasks[e.id] |= TypeRegistry::instance().getMask<T>();
        return array->get(e);
    }

    template<typename T>
    void removeComponent(Entity e) {
        auto* array = getComponentArray<T>();
        if (array) {
            array->remove(e);
            m_entityMasks[e.id] &= ~TypeRegistry::instance().getMask<T>();
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

    // Type-erased component API (for CommandBuffer and reflection)
    void addComponentRaw(Entity e, ComponentTypeID type, const void* data);
    void removeComponentRaw(Entity e, ComponentTypeID type);
    [[nodiscard]] const void* getComponentRaw(Entity e, ComponentTypeID type) const;
    [[nodiscard]] void* getComponentRaw(Entity e, ComponentTypeID type);
    [[nodiscard]] bool hasComponentRaw(Entity e, ComponentTypeID type) const;

    template<typename T>
    [[nodiscard]] ComponentArray<T>* getComponentArray() {
        ComponentTypeID typeID = TypeRegistry::instance().getTypeID<T>();
        auto* v = m_componentArrays.find(typeID);
        if (!v) return nullptr;
        return static_cast<ComponentArray<T>*>(*v);
    }

    template<typename T>
    [[nodiscard]] const ComponentArray<T>* getComponentArray() const {
        ComponentTypeID typeID = TypeRegistry::instance().getTypeID<T>();
        auto* v = m_componentArrays.find(typeID);
        if (!v) return nullptr;
        return static_cast<const ComponentArray<T>*>(*v);
    }

    const HashMap<ComponentTypeID, IComponentArray*>& componentArrays() const {
        return m_componentArrays;
    }

    void collectDescendants(Entity e, DynamicArray<Entity>& out) const;

private:
    template<typename T>
    ComponentArray<T>* getOrCreateComponentArray() {
        ComponentTypeID typeID = TypeRegistry::instance().getTypeID<T>();
        auto* v = m_componentArrays.find(typeID);
        if (v) {
            return static_cast<ComponentArray<T>*>(*v);
        }
        auto* array = new ComponentArray<T>();
        m_componentArrays.insert(typeID, array);
        return array;
    }

    EntityRegistry* m_registry = nullptr;
    bool m_ownsRegistry = false;
    CommandBuffer* m_cmdBuffer = nullptr;
    DynamicArray<u32> m_entityMasks;
    HashMap<ComponentTypeID, IComponentArray*> m_componentArrays;
    DynamicArray<u32> m_entityRanks;
    u64 m_currentFrame = 0;
};

void initCore();
void registerBuiltinTypes();
void printWorld(const World& world);

} // namespace Entelechy
