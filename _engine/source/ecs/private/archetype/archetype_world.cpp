#include "ecs/archetype/archetype_world.h"
#include "core/allocator/allocator.h"
#include <cstring>

namespace Entelechy {

// ------------------------------------------------------------------
// ArchetypeWorld
// ------------------------------------------------------------------

ArchetypeWorld::ArchetypeWorld(IAllocator* allocator)
    : m_allocator(allocator) {}

ArchetypeWorld::~ArchetypeWorld() {
    for (auto pair : m_archetypes) {
        Archetype* archetype = pair.second;
        Chunk* chunk = archetype->firstChunk;
        while (chunk) {
            Chunk* next = chunk->next;
            freeChunk(chunk);
            chunk = next;
        }
        archetype->~Archetype();
        m_allocator->free(archetype);
    }
}

Entity ArchetypeWorld::spawn() {
    Entity e = m_registry.create();
    if (e.id >= m_entity_records.size()) {
        m_entity_records.resize(e.id + 1);
    }
    m_entity_records[e.id] = ArchetypeRecord{};
    return e;
}

void ArchetypeWorld::destroy(Entity e) {
    if (!valid(e)) return;
    ArchetypeRecord& rec = m_entity_records[e.id];
    if (rec.archetype && rec.chunk) {
        Chunk* chunk = rec.chunk;
        u16 idx = rec.indexInChunk;
        u16 last = chunk->entityCount - 1;

        if (idx != last) {
            // Swap with last entity in chunk
            u32* entityIds = reinterpret_cast<u32*>(chunkData(*chunk));
            Entity moved{entityIds[last], m_registry.getGeneration(entityIds[last])};

            // Move all component data for the last entity to idx
            for (usize c = 0; c < rec.archetype->componentTypes.size(); ++c) {
                usize size = rec.archetype->componentSizes[c];
                usize offset = rec.archetype->componentOffsets[c];
                u8* base = chunkData(*chunk) + offset;
                std::memcpy(base + idx * size, base + last * size, size);
            }
            entityIds[idx] = entityIds[last];
            m_entity_records[moved.id].indexInChunk = idx;
        }
        --chunk->entityCount;
        --rec.archetype->entityCount;

        // If chunk became empty and it's not the only chunk, free it
        if (chunk->entityCount == 0 && chunk != rec.archetype->firstChunk) {
            // Unlink from list
            Chunk* prev = rec.archetype->firstChunk;
            while (prev && prev->next != chunk) prev = prev->next;
            if (prev) prev->next = chunk->next;
            if (rec.archetype->lastChunk == chunk) rec.archetype->lastChunk = prev;
            freeChunk(chunk);
        }
    }
    rec = ArchetypeRecord{};
    m_registry.destroy(e);
}

Archetype* ArchetypeWorld::findOrCreateArchetype(ArchetypeID id) {
    if (id == 0) return nullptr;
    auto* v = m_archetypes.find(id);
    if (v) return *v;
    return createArchetype(id);
}

Archetype* ArchetypeWorld::createArchetype(ArchetypeID id) {
    void* mem = m_allocator->allocate(sizeof(Archetype), alignof(Archetype));
    Archetype* archetype = new (mem) Archetype();
    archetype->id = id;

    // Build component type list from bitmask
    for (u32 i = 0; i < 64; ++i) {
        if (id & (1ull << i)) {
            archetype->componentTypes.pushBack(i);
            const ComponentDesc* desc = TypeRegistry::instance().findComponent(i);
            if (desc) {
                archetype->componentSizes.pushBack(desc->size);
                archetype->componentAlignments.pushBack(alignof(std::max_align_t)); // conservative
            } else {
                archetype->componentSizes.pushBack(0);
                archetype->componentAlignments.pushBack(1);
            }
        }
    }

    // Compute SoA offsets inside chunk data
    usize offset = 0;
    // entityIds array first
    usize entityIdsSize = 0; // will be set after computing capacity

    // First pass: compute capacity
    usize totalCompSize = 0;
    for (usize s : archetype->componentSizes) {
        totalCompSize += s;
    }
    usize entitySize = sizeof(u32) + totalCompSize;
    archetype->entitySize = entitySize;
    if (entitySize == 0) {
        archetype->entitiesPerChunk = 0;
    } else {
        archetype->entitiesPerChunk = static_cast<u16>(chunkDataCapacity() / entitySize);
        if (archetype->entitiesPerChunk == 0) archetype->entitiesPerChunk = 1;
    }
    entityIdsSize = archetype->entitiesPerChunk * sizeof(u32);

    // Second pass: compute aligned offsets
    offset = entityIdsSize;
    for (usize i = 0; i < archetype->componentTypes.size(); ++i) {
        usize align = archetype->componentAlignments[i];
        offset = (offset + align - 1) & ~(align - 1);
        archetype->componentOffsets.pushBack(offset);
        offset += archetype->entitiesPerChunk * archetype->componentSizes[i];
    }

    m_archetypes.insert(id, archetype);
    return archetype;
}

Chunk* ArchetypeWorld::allocateChunk(Archetype& archetype) {
    void* mem = m_allocator->allocate(sizeof(Chunk), alignof(Chunk));
    Chunk* chunk = new (mem) Chunk();
    chunk->archetype = archetype.id;
    chunk->entityCapacity = archetype.entitiesPerChunk;
    chunk->entityCount = 0;
    chunk->next = nullptr;
    return chunk;
}

void ArchetypeWorld::freeChunk(Chunk* chunk) {
    chunk->~Chunk();
    m_allocator->free(chunk);
}

void ArchetypeWorld::releaseChunk(Chunk* chunk) {
    // Used when chunk is still in archetype list but we want to recycle
    // For now, just free it
    freeChunk(chunk);
}

void ArchetypeWorld::moveEntityToArchetypeRaw(Entity e, const ArchetypeRecord& oldRec,
                                              Archetype* newArchetype, ComponentTypeID newComp,
                                              const void* newData) {

    // Find or allocate a chunk with space in the new archetype
    Chunk* newChunk = nullptr;
    if (newArchetype) {
        if (!newArchetype->lastChunk || newArchetype->lastChunk->entityCount >= newArchetype->lastChunk->entityCapacity) {
            Chunk* chunk = allocateChunk(*newArchetype);
            if (!newArchetype->firstChunk) {
                newArchetype->firstChunk = chunk;
                newArchetype->lastChunk = chunk;
            } else {
                newArchetype->lastChunk->next = chunk;
                newArchetype->lastChunk = chunk;
            }
        }
        newChunk = newArchetype->lastChunk;
    }

    u16 newIdx = newChunk ? newChunk->entityCount : 0;

    // Copy component data from old archetype
    if (oldRec.archetype && oldRec.chunk && newArchetype) {
        for (usize i = 0; i < oldRec.archetype->componentTypes.size(); ++i) {
            ComponentTypeID type = oldRec.archetype->componentTypes[i];
            usize oldSize = oldRec.archetype->componentSizes[i];
            usize oldOffset = oldRec.archetype->componentOffsets[i];
            u8* oldBase = chunkData(*oldRec.chunk) + oldOffset;

            // Find matching component in new archetype
            usize newCompIdx = findComponentIndex(*newArchetype, type);
            if (newCompIdx != static_cast<usize>(-1)) {
                usize newSize = newArchetype->componentSizes[newCompIdx];
                usize newOffset = newArchetype->componentOffsets[newCompIdx];
                u8* newBase = chunkData(*newChunk) + newOffset;
                std::memcpy(newBase + newIdx * newSize, oldBase + oldRec.indexInChunk * oldSize,
                            oldSize < newSize ? oldSize : newSize);
            }
        }
    }

    // Write new component if provided
    if (newComp != INVALID_COMPONENT_TYPE_ID && newData && newArchetype) {
        usize newCompIdx = findComponentIndex(*newArchetype, newComp);
        if (newCompIdx != static_cast<usize>(-1)) {
            usize newSize = newArchetype->componentSizes[newCompIdx];
            usize newOffset = newArchetype->componentOffsets[newCompIdx];
            u8* newBase = chunkData(*newChunk) + newOffset;
            u8* dest = newBase + newIdx * newSize;
            std::memcpy(dest, newData, newSize);
        }
    }

    // Write entity ID
    if (newChunk) {
        u32* entityIds = reinterpret_cast<u32*>(chunkData(*newChunk));
        entityIds[newIdx] = e.id;
        ++newChunk->entityCount;
        ++newArchetype->entityCount;
    }

    // Remove from old archetype (swap-and-pop)
    if (oldRec.archetype && oldRec.chunk) {
        Chunk* oldChunk = oldRec.chunk;
        u16 idx = oldRec.indexInChunk;
        u16 last = oldChunk->entityCount - 1;
        if (idx != last) {
            u32* oldEntityIds = reinterpret_cast<u32*>(chunkData(*oldChunk));
            Entity moved{oldEntityIds[last], m_registry.getGeneration(oldEntityIds[last])};
            for (usize i = 0; i < oldRec.archetype->componentTypes.size(); ++i) {
                usize size = oldRec.archetype->componentSizes[i];
                usize offset = oldRec.archetype->componentOffsets[i];
                u8* base = chunkData(*oldChunk) + offset;
                std::memcpy(base + idx * size, base + last * size, size);
            }
            oldEntityIds[idx] = oldEntityIds[last];
            m_entity_records[moved.id].indexInChunk = idx;
        }
        --oldChunk->entityCount;
        --oldRec.archetype->entityCount;
    }

    // Update entity record
    m_entity_records[e.id] = ArchetypeRecord{newArchetype, newChunk, newIdx};
}

usize ArchetypeWorld::findComponentIndex(const Archetype& archetype, ComponentTypeID type) const {
    for (usize i = 0; i < archetype.componentTypes.size(); ++i) {
        if (archetype.componentTypes[i] == type) return i;
    }
    return static_cast<usize>(-1);
}

// Type-erased API
void ArchetypeWorld::addComponentRaw(Entity e, ComponentTypeID type, const void* data) {
    CHECK(valid(e));
    ArchetypeRecord oldRec = m_entity_records[e.id];
    u64 mask = (1ull << type);
    u64 newMask = oldRec.archetype ? (oldRec.archetype->id | mask) : mask;
    Archetype* newArchetype = findOrCreateArchetype(newMask);
    moveEntityToArchetypeRaw(e, oldRec, newArchetype, type, data);
}

void ArchetypeWorld::removeComponentRaw(Entity e, ComponentTypeID type) {
    CHECK(valid(e));
    if (!hasComponentRaw(e, type)) return;
    ArchetypeRecord oldRec = m_entity_records[e.id];
    u64 mask = (1ull << type);
    u64 newMask = oldRec.archetype->id & ~mask;
    Archetype* newArchetype = findOrCreateArchetype(newMask);
    moveEntityToArchetypeRaw(e, oldRec, newArchetype, INVALID_COMPONENT_TYPE_ID, nullptr);
}

const void* ArchetypeWorld::getComponentRaw(Entity e, ComponentTypeID type) const {
    if (!valid(e)) return nullptr;
    const ArchetypeRecord& rec = m_entity_records[e.id];
    if (!rec.archetype) return nullptr;
    usize compIndex = findComponentIndex(*rec.archetype, type);
    if (compIndex == static_cast<usize>(-1)) return nullptr;
    usize size = rec.archetype->componentSizes[compIndex];
    usize offset = rec.archetype->componentOffsets[compIndex];
    const u8* base = chunkData(*rec.chunk) + offset;
    return base + rec.indexInChunk * size;
}

bool ArchetypeWorld::hasComponentRaw(Entity e, ComponentTypeID type) const {
    if (!valid(e)) return false;
    const ArchetypeRecord& rec = m_entity_records[e.id];
    if (!rec.archetype) return false;
    return (rec.archetype->id & (1ull << type)) != 0;
}

} // namespace Entelechy
