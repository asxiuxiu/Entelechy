#pragma once
#include "ecs/world/scheduler.h"

namespace Entelechy {

class MovementSystem : public System {
public:
    void tick(World& world, FrameArena& arena, f32 dt) override;
};

} // namespace Entelechy
