#include "ecs/event/event_cleanup_system.h"
#include "ecs/event/event_lifetime.h"
#include "ecs/query/query.h"

namespace Entelechy {

void EventCleanupSystem::tick(World& world, FrameArena& arena, f32 dt) {
    (void)arena; (void)dt;
    u64 currentFrame = world.currentFrame();
    // Collect first, then destroy, to avoid invalidating iterators.
    DynamicArray<Entity> toDestroy;
    for (auto [e, lifetime] : Query<EventLifetime>(world)) {
        if (lifetime && lifetime->frameCreated <= currentFrame) {
            toDestroy.pushBack(e);
        }
    }
    for (Entity e : toDestroy) {
        world.destroy(e);
    }
}

} // namespace Entelechy
