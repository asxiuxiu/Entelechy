#pragma once
#include "ecs/world/world.h"

namespace Entelechy {

// FrustumCullSystem — per-entity frustum culling against the ExtractedView.
// Entities without an AABB are always visible (e.g. skyboxes).
class FrustumCullSystem {
public:
    void run(World& renderWorld);
};

} // namespace Entelechy
