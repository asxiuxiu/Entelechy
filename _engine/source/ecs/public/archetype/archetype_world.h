#pragma once
#include "ecs/type/entity_registry.h"
#include "ecs/type/type_registry.h"
#include "ecs/component/component_array.h"
#include "core/container/dynamic_array.h"
#include "core/container/hash_map.h"
#include "core/allocator/allocator.h"
#include "ecs/storage/archetype_chunk.h"
#include <tuple>
#include <type_traits>

namespace Entelechy {

// Forward declaration
struct Archetype;

// ------------------------------------------------------------------
// ArchetypeRecord — maps Entity → (Archetype, Chunk, slot)
// ------------------------------------------------------------------
struct ArchetypeRecord {
    Archetype* archetype = nullptr;
    Chunk* chunk = nullptr;
    u16 indexInChunk = 0;
};

// ------------------------------------------------------------------
// Archetype — a unique component combination.
// Owns a linked list of Chunks that store entities of this archetype.
// ------------------------------------------------------------------
struct Archetype {
    ArchetypeID id = 0;
    DynamicArray<ComponentTypeID> componentTypes;
    DynamicArray<usize> componentSizes;
    DynamicArray<usize> componentAlignments;
    DynamicArray<usize> componentOffsets; // byte offsets inside chunk storage (per entity)
    usize entitySize = 0;       // total bytes per entity inside chunk
    u16 entitiesPerChunk = 0;   // derived from chunk capacity
    Chunk* firstChunk = nullptr;
    Chunk* lastChunk = nullptr;
    usize entityCount = 0;      // total entities across all chunks
};

// Chunk is defined in ecs/storage/archetype_chunk.h.
// We add helper methods via free functions to avoid redefinition.
inline u8* chunkData(Chunk& chunk) { return chunk.storage; }
inline const u8* chunkData(const Chunk& chunk) { return chunk.storage; }
inline constexpr usize chunkDataCapacity() {
    return Chunk::CAPACITY > sizeof(Chunk) ? Chunk::CAPACITY - sizeof(Chunk) : 0;
}

// ------------------------------------------------------------------
// ArchetypeWorld — chunk-based ECS storage.
// ------------------------------------------------------------------
class ArchetypeWorld {
public:
    explicit ArchetypeWorld(IAllocator* allocator = GetGlobalAllocator());
    ~ArchetypeWorld();

    [[nodiscard]] Entity spawn();
    void destroy(Entity e);

    template<typename T>
    T* addComponent(Entity e, const T& value) {
        CHECK(valid(e));
        if (hasComponent<T>(e)) {
            T* ptr = getComponent<T>(e);
            *ptr = value;
            return ptr;
        }
        ArchetypeRecord oldRec = m_entity_records[e.id];
        u64 newMask = oldRec.archetype ? (oldRec.archetype->id | TypeRegistry::instance().getMask<T>())
                                       : TypeRegistry::instance().getMask<T>();
        Archetype* newArchetype = findOrCreateArchetype(newMask);
        ComponentTypeID type = TypeRegistry::instance().getTypeID<T>();
        moveEntityToArchetypeRaw(e, oldRec, newArchetype, type, &value);
        return getComponent<T>(e);
    }

    template<typename T>
    void removeComponent(Entity e) {
        CHECK(valid(e));
        if (!hasComponent<T>(e)) return;
        ArchetypeRecord oldRec = m_entity_records[e.id];
        u64 newMask = oldRec.archetype->id & ~TypeRegistry::instance().getMask<T>();
        Archetype* newArchetype = findOrCreateArchetype(newMask);
        moveEntityToArchetypeRaw(e, oldRec, newArchetype, INVALID_COMPONENT_TYPE_ID, nullptr);
    }

    template<typename T>
    [[nodiscard]] T* getComponent(Entity e) {
        if (!valid(e)) return nullptr;
        const ArchetypeRecord& rec = m_entity_records[e.id];
        if (!rec.archetype) return nullptr;
        usize compIndex = findComponentIndex(*rec.archetype, TypeRegistry::instance().getTypeID<T>());
        if (compIndex == static_cast<usize>(-1)) return nullptr;
        return getComponentData<T>(*rec.chunk, rec.indexInChunk, compIndex);
    }

    template<typename T>
    [[nodiscard]] const T* getComponent(Entity e) const {
        if (!valid(e)) return nullptr;
        const ArchetypeRecord& rec = m_entity_records[e.id];
        if (!rec.archetype) return nullptr;
        usize compIndex = findComponentIndex(*rec.archetype, TypeRegistry::instance().getTypeID<T>());
        if (compIndex == static_cast<usize>(-1)) return nullptr;
        return getComponentData<T>(*rec.chunk, rec.indexInChunk, compIndex);
    }

    template<typename T>
    [[nodiscard]] bool hasComponent(Entity e) const {
        if (!valid(e)) return false;
        const ArchetypeRecord& rec = m_entity_records[e.id];
        if (!rec.archetype) return false;
        return (rec.archetype->id & TypeRegistry::instance().getMask<T>()) != 0;
    }

    [[nodiscard]] bool valid(Entity e) const {
        return m_registry.isAlive(e);
    }

    [[nodiscard]] u32 getEntityGeneration(u32 id) const {
        return m_registry.getGeneration(id);
    }

    [[nodiscard]] usize entityCount() const { return m_registry.aliveCount(); }

    // Type-erased component API
    void addComponentRaw(Entity e, ComponentTypeID type, const void* data);
    void removeComponentRaw(Entity e, ComponentTypeID type);
    [[nodiscard]] const void* getComponentRaw(Entity e, ComponentTypeID type) const;
    [[nodiscard]] bool hasComponentRaw(Entity e, ComponentTypeID type) const;

    // Query API
    template<typename... Cs>
    auto query();

private:
    Archetype* findOrCreateArchetype(ArchetypeID id);
    Archetype* createArchetype(ArchetypeID id);
    Chunk* allocateChunk(Archetype& archetype);
    void freeChunk(Chunk* chunk);
    void releaseChunk(Chunk* chunk);

    // Move entity from oldRec to newArchetype
    void moveEntityToArchetypeRaw(Entity e, const ArchetypeRecord& oldRec,
                                  Archetype* newArchetype, ComponentTypeID newComp, const void* newData);

    [[nodiscard]] usize findComponentIndex(const Archetype& archetype, ComponentTypeID type) const;

    // Grant QueryArchetype access to private members for iteration
    template<typename... Cs>
    friend class QueryArchetype;

    template<typename T>
    [[nodiscard]] T* getComponentData(Chunk& chunk, u16 indexInChunk, usize compIndex) {
        Archetype* archetype = nullptr;
        // Find archetype via hashmap (linear search for now — archetype count is small)
        for (auto pair : m_archetypes) {
            if (pair.second->id == chunk.archetype) {
                archetype = pair.second;
                break;
            }
        }
        CHECK(archetype != nullptr);
        u8* base = chunkData(chunk) + archetype->componentOffsets[compIndex];
        return reinterpret_cast<T*>(base + indexInChunk * archetype->componentSizes[compIndex]);
    }

    template<typename T>
    [[nodiscard]] const T* getComponentData(const Chunk& chunk, u16 indexInChunk, usize compIndex) const {
        return const_cast<ArchetypeWorld*>(this)->getComponentData<T>(
            const_cast<Chunk&>(chunk), indexInChunk, compIndex);
    }

    IAllocator* m_allocator = nullptr;
    EntityRegistry m_registry;
    HashMap<ArchetypeID, Archetype*> m_archetypes;
    DynamicArray<ArchetypeRecord> m_entity_records;
};

// ------------------------------------------------------------------
// QueryArchetype — iterate matching archetypes directly over chunks.
// ------------------------------------------------------------------
template<typename... Cs>
class QueryArchetype {
public:
    explicit QueryArchetype(ArchetypeWorld& world);

    struct Iterator {
        ArchetypeWorld* world;
        const HashMap<ArchetypeID, Archetype*>* archetypes;
        DynamicArray<Archetype*> matching;
        usize archetypeIndex;
        Chunk* currentChunk;
        u16 slotIndex;
        u64 requiredMask;

        void advanceToNextValid();

        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;
        void operator++();

        using value_type = std::tuple<Entity, Cs*...>;
        value_type operator*() const;
    };

    Iterator begin();
    Iterator end();

private:
    ArchetypeWorld* m_world;
    u64 m_requiredMask = 0;
};

} // namespace Entelechy

// Template implementations need to be in header
#include "ecs/archetype/archetype_world.inl"
