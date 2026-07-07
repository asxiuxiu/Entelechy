#pragma once
#include "ecs/type/entity_registry.h"
#include "ecs/component/component_array.h"
#include "ecs/type/type_registry.h"
#include "core/container/dynamic_array.h"
#include "core/container/hash_map.h"
#include "core/allocator/allocator.h"
#include <cstdio>
#include <tuple>
#include <memory>
#include <utility>
#include <type_traits>

namespace Entelechy
{

class CommandBuffer;

class World
{
public:
    explicit World(IAllocator *allocator = GetGlobalAllocator());
    explicit World(EntityRegistry &registry, IAllocator *allocator = GetGlobalAllocator());
    ~World();

    void bindCommandBuffer(CommandBuffer *cmdBuffer)
    {
        m_cmd_buffer = cmdBuffer;
    }

    [[nodiscard]] Entity spawn()
    {
        CHECK(m_registry != nullptr);
        Entity e = m_registry->create();
        if (e.id >= m_entity_masks.size())
        {
            m_entity_masks.resize(e.id + 1, 0);
        }
        if (e.id >= m_entity_ranks.size())
        {
            m_entity_ranks.resize(e.id + 1, 0);
        }
        m_entity_masks[e.id] = 0;
        m_entity_ranks[e.id] = 0;
        return e;
    }

    [[nodiscard]] DynamicArray<Entity> spawnBatch(usize count);

    void destroy(Entity e);
    void destroyImmediate(Entity e);

    // Batch destroy all entities and clear all component arrays.
    // Used by RenderWorld::clear() and other bulk-reset scenarios.
    void clearAllEntities();

    [[nodiscard]] bool valid(Entity e) const
    {
        return m_registry && m_registry->isAlive(e);
    }

    [[nodiscard]] usize entityCount() const
    {
        return m_registry ? m_registry->aliveCount() : 0;
    }

    [[nodiscard]] usize maxEntityID() const
    {
        return m_registry ? m_registry->capacity() : 0;
    }

    [[nodiscard]] u32 getEntityGeneration(u32 id) const
    {
        return m_registry ? m_registry->getGeneration(id) : 0xFFFFFFFFu;
    }

    [[nodiscard]] u64 getEntityMask(u32 id) const
    {
        if (id < m_entity_masks.size())
            return m_entity_masks[id];
        return 0;
    }

    void setCurrentFrame(u64 frame)
    {
        m_current_frame = frame;
    }
    [[nodiscard]] u64 currentFrame() const
    {
        return m_current_frame;
    }

    void setParent(Entity child, Entity parent);
    void setParentImmediate(Entity child, Entity parent);

    // KeepWorld semantics: preserves the child's world transform when re-parenting.
    // The child's local transform is recomputed so that its global position
    // remains unchanged after the hierarchy change.
    void setParentInPlace(Entity child, Entity parent);

    [[nodiscard]] u32 getRank(Entity e) const;
    void setRank(Entity e, u32 rank);

    template <typename T>
    T *addComponent(Entity e, const T &comp)
    {
        auto *array = getOrCreateComponentArray<T>();
        array->set(e, comp);
        m_entity_masks[e.id] |= TypeRegistry::instance().getMask<T>();
        return array->get(e);
    }

    template <typename T>
    std::enable_if_t<std::is_rvalue_reference_v<T &&>, T *> addComponent(Entity e, T &&comp)
    {
        auto *array = getOrCreateComponentArray<T>();
        array->set(e, std::forward<T>(comp));
        m_entity_masks[e.id] |= TypeRegistry::instance().getMask<T>();
        return array->get(e);
    }

    template <typename T>
    void removeComponent(Entity e)
    {
        auto *array = getComponentArray<T>();
        if (array)
        {
            array->remove(e);
            m_entity_masks[e.id] &= ~TypeRegistry::instance().getMask<T>();
        }
    }

    template <typename T>
    [[nodiscard]] T *getComponent(Entity e)
    {
        auto *array = getComponentArray<T>();
        if (!array)
            return nullptr;
        return array->get(e);
    }

    template <typename T>
    [[nodiscard]] const T *getComponent(Entity e) const
    {
        auto *array = getComponentArray<T>();
        if (!array)
            return nullptr;
        return array->get(e);
    }

    template <typename T>
    [[nodiscard]] bool hasComponent(Entity e) const
    {
        auto *array = getComponentArray<T>();
        if (!array)
            return false;
        return array->has(e);
    }

    // Type-erased component API (for CommandBuffer and reflection)
    void addComponentRaw(Entity e, ComponentTypeID type, const void *data);
    void removeComponentRaw(Entity e, ComponentTypeID type);
    [[nodiscard]] const void *getComponentRaw(Entity e, ComponentTypeID type) const;
    [[nodiscard]] void *getComponentRaw(Entity e, ComponentTypeID type);
    [[nodiscard]] bool hasComponentRaw(Entity e, ComponentTypeID type) const;

    template <typename T>
    [[nodiscard]] ComponentArray<T> *getComponentArray()
    {
        ComponentTypeID typeID = TypeRegistry::instance().getTypeID<T>();
        auto *v = m_component_arrays.find(typeID);
        if (!v)
            return nullptr;
        return static_cast<ComponentArray<T> *>(*v);
    }

    template <typename T>
    [[nodiscard]] const ComponentArray<T> *getComponentArray() const
    {
        ComponentTypeID typeID = TypeRegistry::instance().getTypeID<T>();
        auto *v = m_component_arrays.find(typeID);
        if (!v)
            return nullptr;
        return static_cast<const ComponentArray<T> *>(*v);
    }

    const HashMap<ComponentTypeID, IComponentArray *> &componentArrays() const
    {
        return m_component_arrays;
    }

    void collectDescendants(Entity e, DynamicArray<Entity> &out) const;

private:
    template <typename T>
    ComponentArray<T> *getOrCreateComponentArray()
    {
        ComponentTypeID typeID = TypeRegistry::instance().getTypeID<T>();
        auto *v = m_component_arrays.find(typeID);
        if (v)
        {
            return static_cast<ComponentArray<T> *>(*v);
        }
        void *mem = m_allocator->allocate(sizeof(ComponentArray<T>), alignof(ComponentArray<T>));
        auto *array = new (mem) ComponentArray<T>();
        m_component_arrays.insert(typeID, array);
        return array;
    }

    IAllocator *m_allocator = nullptr;
    EntityRegistry *m_registry = nullptr;
    bool m_owns_registry = false;
    CommandBuffer *m_cmd_buffer = nullptr;
    DynamicArray<u64> m_entity_masks;
    HashMap<ComponentTypeID, IComponentArray *> m_component_arrays;
    DynamicArray<u32> m_entity_ranks;
    u64 m_current_frame = 0;
};

void initEcs();
void registerBuiltinTypes();
void printWorld(const World &world);

} // namespace Entelechy
