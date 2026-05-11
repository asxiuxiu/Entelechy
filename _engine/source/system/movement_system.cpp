#include "movement_system.h"
#include "query.h"

namespace Entelechy {

void MovementSystem::tick(World& world, FrameArena& arena, f32 dt) {
    (void)arena;
    for (auto [e, pos, vel] : Query<Position, Velocity>(world)) {
        if (pos && vel) {
            pos->x += vel->vx * dt;
            pos->y += vel->vy * dt;
        }
    }
}

} // namespace Entelechy
