#pragma once
#include "foundation_types.h"
#include "entity_registry.h"
#include "type_registry.h"
#include "dynamic_array.h"
#include "hash_map.h"

namespace Entelechy {

class World;

// ------------------------------------------------------------------
// PrefabAsset -- serializable blueprint for entity instantiation
// ------------------------------------------------------------------
// Stores component data in a type-erased, serializable form.
// Each PrefabAsset can contain multiple entities (for prefab hierarchies).
// ------------------------------------------------------------------
struct PrefabEntityEntry {
    // Component types and their serialized data (parallel arrays)
    DynamicArray<ComponentTypeID> componentTypes;
    DynamicArray<DynamicArray<u8>> componentData; // parallel to componentTypes

    // Parent entity index within the prefab, or 0xFFFFFFFFu for root
    u32 parentIndex = 0xFFFFFFFFu;
};

struct PrefabAsset {
    SmallString name;
    DynamicArray<PrefabEntityEntry> entities;

    // The root entity is always at index 0.
    [[nodiscard]] bool valid() const { return !entities.empty(); }
};

// ------------------------------------------------------------------
// PrefabInstance -- ECS component marking an entity as a prefab instance
// ------------------------------------------------------------------
struct PrefabInstance {
    u32 prefabAssetID = 0xFFFFFFFFu;
    u32 entityIndex = 0; // index within the prefab's entity array
};

REFLECT_COMPONENT(PrefabInstance,
    REG_FIELD(PrefabInstance, prefabAssetID, u32),
    REG_FIELD(PrefabInstance, entityIndex, u32)
)

// ------------------------------------------------------------------
// PrefabOverride -- per-instance component override
// ------------------------------------------------------------------
struct PrefabOverride {
    ComponentTypeID type = INVALID_COMPONENT_TYPE_ID;
    DynamicArray<u8> data;
};

// ------------------------------------------------------------------
// PrefabSystem -- instantiation and override management
//
// Design: Branch C (instantiate-and-merge)
//   - PrefabAsset lives outside ECS (managed by PrefabSystem)
//   - PrefabInstance component marks instantiated entities
//   - Overrides are stored in PrefabSystem and applied after instantiation
// ------------------------------------------------------------------
class PrefabSystem {
public:
    // Register a prefab asset. Returns asset ID.
    u32 registerAsset(PrefabAsset asset);

    // Instantiate a prefab into the world. Returns the root entity.
    // If parent is valid, the root entity is attached as a child.
    Entity instantiate(u32 prefabAssetID, World& world, Entity parent = Entity{0xFFFFFFFFu, 0});

    // Apply overrides to an instance entity.
    void applyOverrides(Entity instance, World& world, const DynamicArray<PrefabOverride>& overrides);

    // Get registered asset by ID.
    [[nodiscard]] const PrefabAsset* findAsset(u32 id) const;

    [[nodiscard]] usize assetCount() const { return m_assets.size(); }

private:
    DynamicArray<PrefabAsset> m_assets;
    u32 m_nextAssetID = 0;
};

} // namespace Entelechy
