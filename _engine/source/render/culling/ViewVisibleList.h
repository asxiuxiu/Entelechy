#pragma once
#include "foundation_types.h"
#include "dynamic_array.h"
#include "entity_registry.h"

namespace Entelechy {

// ViewVisibleList — explicit ECS output of the Culling stage.
// Today: CPU brute-force frustum culling. Tomorrow: BVH. The downstream is unchanged.
struct ViewVisibleList {
    DynamicArray<Entity> entities;
};

} // namespace Entelechy
