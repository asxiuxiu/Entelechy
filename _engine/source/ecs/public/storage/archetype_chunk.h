#pragma once
#include "core/foundation_types.h"

namespace Entelechy
{

// ========================================================================
// Archetype Chunk Storage — Interface Sketch (Phase 4.1 Pre-research)
// ========================================================================
// This header contains ONLY forward-looking sketches and does not affect
// the current ECS runtime. It exists so Phase C→D migration can reference
// a concrete target shape.
//
// Design decisions (from knowledge base):
//   - Fixed 16 KiB chunks (fits in L1/L2 cache lines)
//   - SoA (Structure of Arrays) per component type inside chunk
//   - Entities with the same ArchetypeID live in the same chunk list
//   - ArchetypeID = bitmask of component types (up to 64 bits)
// ========================================================================

using ArchetypeID = u64;

// Chunk header — 64-byte aligned so that the payload area after it is also
// cache-line aligned.  The payload is NOT a struct member; it lives in the
// same allocation immediately after the header.
struct alignas(64) Chunk
{
    static constexpr usize CAPACITY = 16 * 1024; // 16 KiB payload

    ArchetypeID archetype = 0;
    struct Archetype *archetypePtr = nullptr; // fast reverse lookup
    Chunk *next = nullptr;                    // intrusive linked list
    u16 entityCount = 0;                      // how many entities currently live here
    u16 entityCapacity = 0;                   // derived from component sizes
};

// Helper: derive ArchetypeID from a set of ComponentTypeIDs.
// In the current 32-component system this is simply a u32 mask stored in u64.
inline ArchetypeID makeArchetypeID(u32 componentMask)
{
    return static_cast<ArchetypeID>(componentMask);
}

// Future World storage shape (commented to avoid breaking current build):
//
//   HashMap<ArchetypeID, ChunkList> m_archetypeChunks;
//   HashMap<EntityID, ArchetypeRecord> m_entityArchetypes;
//
// Where ArchetypeRecord = { Chunk* chunk; u16 indexInChunk; }.
//
// Migration path from ComponentArray<T> (current SparseSet+Column):
//   1. Keep SparseSet+Column for Phase C/D as the stable backend.
//   2. When Archetype storage lands, ComponentArray becomes a compatibility
//      view over Chunk columns for legacy Query<> paths.
//   3. New QueryArchetype<> iterates Chunk lists directly.

} // namespace Entelechy
