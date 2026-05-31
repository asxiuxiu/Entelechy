#pragma once
#include "ecs/archetype/archetype_world.h"

namespace Entelechy {

// ------------------------------------------------------------------
// QueryArchetype implementation
// ------------------------------------------------------------------

template<typename... Cs>
QueryArchetype<Cs...>::QueryArchetype(ArchetypeWorld& world)
    : m_world(&world)
    , m_requiredMask((TypeRegistry::instance().getMask<Cs>() | ...)) {}

template<typename... Cs>
void QueryArchetype<Cs...>::Iterator::advanceToNextValid() {
    while (true) {
        if (!currentChunk) {
            // Move to next archetype
            while (archetypeIndex < matching.size()) {
                Archetype* archetype = matching[archetypeIndex];
                currentChunk = archetype->firstChunk;
                slotIndex = 0;
                ++archetypeIndex;
                if (currentChunk && currentChunk->entityCount > 0) {
                    return;
                }
            }
            return; // end
        }
        if (slotIndex < currentChunk->entityCount) {
            return;
        }
        // Move to next chunk
        currentChunk = currentChunk->next;
        slotIndex = 0;
    }
}

template<typename... Cs>
bool QueryArchetype<Cs...>::Iterator::operator==(const Iterator& other) const {
    return currentChunk == other.currentChunk && slotIndex == other.slotIndex;
}

template<typename... Cs>
bool QueryArchetype<Cs...>::Iterator::operator!=(const Iterator& other) const {
    return !(*this == other);
}

template<typename... Cs>
void QueryArchetype<Cs...>::Iterator::operator++() {
    ++slotIndex;
    advanceToNextValid();
}

template<typename... Cs>
auto QueryArchetype<Cs...>::Iterator::operator*() const -> value_type {
    u32* entityIds = reinterpret_cast<u32*>(chunkData(*currentChunk));
    Entity e{entityIds[slotIndex], world->getEntityGeneration(entityIds[slotIndex])};

    // Find archetype for this chunk
    Archetype* archetype = nullptr;
    for (auto pair : *archetypes) {
        if (pair.second->id == currentChunk->archetype) {
            archetype = pair.second;
            break;
        }
    }
    CHECK(archetype != nullptr);

    // Build component pointers
    auto getPtr = [&]<typename C>() -> C* {
        ComponentTypeID type = TypeRegistry::instance().getTypeID<C>();
        usize compIndex = static_cast<usize>(-1);
        for (usize i = 0; i < archetype->componentTypes.size(); ++i) {
            if (archetype->componentTypes[i] == type) {
                compIndex = i;
                break;
            }
        }
        CHECK(compIndex != static_cast<usize>(-1));
        usize offset = archetype->componentOffsets[compIndex];
        u8* base = chunkData(*currentChunk) + offset;
        return reinterpret_cast<C*>(base + slotIndex * archetype->componentSizes[compIndex]);
    };

    return std::make_tuple(e, getPtr.template operator()<Cs>()...);
}

template<typename... Cs>
auto QueryArchetype<Cs...>::begin() -> Iterator {
    Iterator it;
    it.world = m_world;
    it.archetypes = &m_world->m_archetypes;
    it.requiredMask = m_requiredMask;
    it.archetypeIndex = 0;
    it.currentChunk = nullptr;
    it.slotIndex = 0;

    // Pre-filter matching archetypes
    for (auto pair : m_world->m_archetypes) {
        Archetype* archetype = pair.second;
        if ((archetype->id & m_requiredMask) == m_requiredMask) {
            it.matching.pushBack(archetype);
        }
    }

    it.advanceToNextValid();
    return it;
}

template<typename... Cs>
auto QueryArchetype<Cs...>::end() -> Iterator {
    Iterator it;
    it.world = m_world;
    it.archetypes = &m_world->m_archetypes;
    it.requiredMask = m_requiredMask;
    it.archetypeIndex = static_cast<usize>(-1);
    it.currentChunk = nullptr;
    it.slotIndex = 0;
    return it;
}

// ------------------------------------------------------------------
// ArchetypeWorld::query
// ------------------------------------------------------------------

template<typename... Cs>
auto ArchetypeWorld::query() {
    return QueryArchetype<Cs...>(*this);
}

} // namespace Entelechy
