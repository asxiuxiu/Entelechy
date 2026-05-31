#pragma once
#include "core/foundation_types.h"
#include "ecs/world/world.h"
#include "ecs/type/types.h"
#include "core/container/dynamic_array.h"

namespace Entelechy {

// ------------------------------------------------------------------
// Hierarchy helpers
// ------------------------------------------------------------------

[[nodiscard]] inline i32 hierarchyDepth(World& world, Entity e) {
    i32 depth = 0;
    Entity current = e;
    while (true) {
        auto* childOf = world.getComponent<ChildOf>(current);
        if (!childOf || !childOf->parent.valid() || !world.valid(childOf->parent)) break;
        current = childOf->parent;
        ++depth;
        if (depth > 10000) break; // cycle guard
    }
    return depth;
}

inline void attachChild(World& world, Entity parent, Entity child) {
    world.setParent(child, parent);
}

inline void detachChild(World& world, Entity child) {
    world.setParent(child, Entity{});
}

[[nodiscard]] inline bool isDescendant(World& world, Entity ancestor, Entity descendant) {
    Entity current = descendant;
    while (true) {
        auto* childOf = world.getComponent<ChildOf>(current);
        if (!childOf || !childOf->parent.valid()) return false;
        if (childOf->parent == ancestor) return true;
        current = childOf->parent;
    }
}

inline void collectDescendants(World& world, Entity e, DynamicArray<Entity>& out) {
    const Children* children = world.getComponent<Children>(e);
    if (!children) return;
    for (const auto& child : children->entities) {
        out.pushBack(child);
        collectDescendants(world, child, out);
    }
}

} // namespace Entelechy
