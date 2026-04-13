#include "world.h"
#include "type_registry.h"
#include <cstdio>

namespace Entelechy {

void initCore() {
    printf("[Entelechy::core] initialized\n");
    // Component types are now auto-registered via REFLECT_COMPONENT macros
    registerBuiltinTypes();
}

void registerBuiltinTypes() {
    // No-op: all builtin components register themselves statically
    printf("[Entelechy::core] registered %zu component types\n", TypeRegistry::instance().componentCount());
}

} // namespace Entelechy

void printWorld(const World& world) {
    printf("=== World State (entities: %zu) ===\n", world.entityCount());
    for (Entity e = 0; e < world.m_alive.size(); ++e) {
        if (world.valid(e)) {
            const auto& p = world.m_positions[e];
            const auto& v = world.m_velocities[e];
            printf("Entity %u: Position(%.1f, %.1f) Velocity(%.1f, %.1f)\n", e, p.x, p.y, v.vx, v.vy);
        }
    }
    printf("\n");
}
