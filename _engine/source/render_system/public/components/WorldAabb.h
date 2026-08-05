#pragma once
#include "core/math/aabb.h"

namespace Entelechy
{

// WorldAABB — main-world component carrying an entity's world-space
// bounds, consumed by frustum culling (and later depth sorting). It
// wraps the pure math AABB so core/math stays free of ECS dependencies;
// the render-world counterpart extracted each frame is RenderAABB.
struct WorldAABB
{
    AABB box;
};

} // namespace Entelechy
