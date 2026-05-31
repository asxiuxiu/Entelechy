#include "ecs/prefab/prefab.h"
#include "ecs/world/world.h"
#include "ecs/component/component_array.h"

namespace Entelechy {

u32 PrefabSystem::registerAsset(PrefabAsset asset) {
    u32 id = m_next_asset_id++;
    if (id >= m_assets.size()) {
        m_assets.resize(id + 1);
    }
    m_assets[id] = std::move(asset);
    return id;
}

const PrefabAsset* PrefabSystem::findAsset(u32 id) const {
    if (id >= m_assets.size()) return nullptr;
    return &m_assets[id];
}

Entity PrefabSystem::instantiate(u32 prefabAssetID, World& world, Entity parent) {
    const PrefabAsset* asset = findAsset(prefabAssetID);
    if (!asset || !asset->valid()) {
        return Entity{0xFFFFFFFFu, 0};
    }

    // Spawn all entities first
    DynamicArray<Entity> spawned;
    spawned.reserve(asset->entities.size());
    for (usize i = 0; i < asset->entities.size(); ++i) {
        Entity e = world.spawn();
        spawned.pushBack(e);
    }

    // Set up hierarchy
    for (usize i = 0; i < asset->entities.size(); ++i) {
        const auto& entry = asset->entities[i];
        if (entry.parentIndex != 0xFFFFFFFFu && entry.parentIndex < spawned.size()) {
            world.setParentImmediate(spawned[i], spawned[entry.parentIndex]);
        }
    }

    // If an external parent was provided, attach the root
    if (parent.valid()) {
        world.setParentImmediate(spawned[0], parent);
    }

    // Add components and mark as prefab instance
    for (usize i = 0; i < asset->entities.size(); ++i) {
        const auto& entry = asset->entities[i];
        Entity e = spawned[i];
        for (usize c = 0; c < entry.componentTypes.size(); ++c) {
            ComponentTypeID type = entry.componentTypes[c];
            const auto& data = entry.componentData[c];
            if (!data.empty()) {
                world.addComponentRaw(e, type, data.data());
            }
        }
        world.addComponent(e, PrefabInstance{prefabAssetID, static_cast<u32>(i)});
    }

    return spawned[0];
}

void PrefabSystem::applyOverrides(Entity instance, World& world, const DynamicArray<PrefabOverride>& overrides) {
    if (!world.valid(instance)) return;
    for (const auto& ov : overrides) {
        if (ov.type == INVALID_COMPONENT_TYPE_ID) continue;
        if (!ov.data.empty()) {
            world.addComponentRaw(instance, ov.type, ov.data.data());
        }
    }
}

} // namespace Entelechy
