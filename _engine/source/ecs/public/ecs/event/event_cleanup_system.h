#pragma once
#include "ecs/world/scheduler.h"

namespace Entelechy {

// Runs in the Last phase. Destroys all event entities whose frameCreated
// is <= the current scheduler frame, ensuring events live exactly one tick.
class EventCleanupSystem : public System {
public:
    void tick(World& world, FrameArena& arena, f32 dt) override;
};

} // namespace Entelechy
