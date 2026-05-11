#pragma once
#include "scheduler.h"

namespace Entelechy {

class TransformPropagationSystem : public System {
public:
    void tick(World& world, FrameArena& arena, f32 dt) override;
};

} // namespace Entelechy
