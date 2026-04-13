#include "movement_system.h"

namespace Entelechy {

void MovementSystem::tick(World& world, float dt) {
    for (Entity e = 0; e < world.m_alive.size(); ++e) {
        if (world.valid(e)) {
            world.m_positions[e].x += world.m_velocities[e].vx * dt;
            world.m_positions[e].y += world.m_velocities[e].vy * dt;
        }
    }
}

} // namespace Entelechy
