#pragma once
#include "scheduler.h"

namespace Entelechy {

class MovementSystem : public System {
public:
    void tick(World& world, FrameArena& arena, float dt) override;
};

} // namespace Entelechy
