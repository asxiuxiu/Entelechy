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

void printWorld(const World& world) {
    printf("=== World State (entities: %zu) ===\n", world.entityCount());
    for (uint32_t id = 0; id < world.maxEntityID(); ++id) {
        Entity e{id, world.getEntityGeneration(id)};
        if (world.valid(e)) {
            const auto* pos = world.getComponent<Position>(e);
            const auto* vel = world.getComponent<Velocity>(e);
            const auto* health = world.getComponent<Health>(e);
            printf("Entity %u: ", id);
            if (pos) printf("Position(%.1f, %.1f) ", pos->x, pos->y);
            if (vel) printf("Velocity(%.1f, %.1f) ", vel->vx, vel->vy);
            if (health) printf("Health(%.1f) ", health->hp);
            printf("\n");
        }
    }
    printf("\n");
}

} // namespace Entelechy
