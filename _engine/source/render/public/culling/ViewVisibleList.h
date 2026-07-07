#pragma once
#include "core/foundation_types.h"
#include "core/container/dynamic_array.h"
#include "ecs/type/entity_registry.h"
#include "ecs/type/type_registry.h"

namespace Entelechy
{

// ViewVisibleList — explicit ECS output of the Culling stage.
// Today: CPU brute-force frustum culling. Tomorrow: BVH. The downstream is unchanged.
struct ViewVisibleList
{
    DynamicArray<Entity> entities;
};

REFLECT_COMPONENT(ViewVisibleList, REG_FIELD(ViewVisibleList, entities, DynamicArray<Entity>))

} // namespace Entelechy
