#include "ecs/system/transform_propagation_system.h"
#include "ecs/query/query.h"
#include "ecs/component/transform_component.h"
#include "core/math/mat4.h"
#include "ecs/hierarchy/hierarchy.h"
#include "thread_pool/thread_pool.h"

namespace Entelechy {

void TransformPropagationSystem::tick(World& world, FrameArena& arena, f32 dt) {
    (void)arena;
    (void)dt;

    // ------------------------------------------------------------------
    // Phase 1: Mark dirty trees.
    // Any entity whose Transform.dirty == 1 gets TransformTreeChanged,
    // and the marker propagates up the parent chain to root.
    // ------------------------------------------------------------------
    for (auto [e, trans] : Query<Transform>(world)) {
        if (!trans || !trans->dirty) continue;
        trans->dirty = 0;

        Entity current = e;
        while (current.valid()) {
            if (!world.hasComponent<TransformTreeChanged>(current)) {
                world.addComponent(current, TransformTreeChanged{});
            }
            auto* childOf = world.getComponent<ChildOf>(current);
            if (!childOf || !childOf->parent.valid()) break;
            current = childOf->parent;
        }
    }

    // ------------------------------------------------------------------
    // Phase 2: Collect only entities marked for update.
    // ------------------------------------------------------------------
    struct Entry {
        Entity e;
        Transform* trans;
        GlobalTransform* global;
        u32 rank;
    };

    DynamicArray<Entry> entries;
    for (auto [e, trans, global] : Query<Transform, GlobalTransform>(world)) {
        if (!trans || !global) continue;
        if (!world.hasComponent<TransformTreeChanged>(e)) continue;
        entries.pushBack({e, trans, global, world.getRank(e)});
    }

    if (entries.empty()) return;

    // ------------------------------------------------------------------
    // Phase 3: Build rank buckets (O(n) instead of O(n log n) sort)
    // ------------------------------------------------------------------
    u32 maxRank = 0;
    for (const auto& entry : entries) {
        if (entry.rank > maxRank) maxRank = entry.rank;
    }

    DynamicArray<DynamicArray<Entity>> buckets;
    buckets.resize(maxRank + 1);
    for (const auto& entry : entries) {
        buckets[entry.rank].pushBack(entry.e);
    }

    // ------------------------------------------------------------------
    // Phase 4: Propagate local → world by rank, parents before children.
    // Within the same rank, entities have no parent-child dependency
    // and can be processed in parallel.
    // ------------------------------------------------------------------
    for (u32 r = 0; r <= maxRank; ++r) {
        auto& bucket = buckets[r];
        if (bucket.empty()) continue;

        auto propagateOne = [&](Entity e) {
            Transform* trans = world.getComponent<Transform>(e);
            GlobalTransform* global = world.getComponent<GlobalTransform>(e);
            if (!trans || !global) return;

            Mat4 local = Mat4::fromTRS(trans->translation, trans->rotation, trans->scale);

            auto* childOf = world.getComponent<ChildOf>(e);
            if (childOf && childOf->parent.valid() && world.valid(childOf->parent)) {
                auto* parentGlobal = world.getComponent<GlobalTransform>(childOf->parent);
                if (parentGlobal) {
                    global->matrix = parentGlobal->matrix * local;
                } else {
                    global->matrix = local;
                }
            } else {
                global->matrix = local;
            }
        };

        if (m_thread_pool && bucket.size() > 16) {
            m_thread_pool->parallelFor(bucket.size(), 16, [&](usize i) {
                propagateOne(bucket[i]);
            });
        } else {
            for (Entity e : bucket) {
                propagateOne(e);
            }
        }
    }

    // ------------------------------------------------------------------
    // Phase 5: Clean up dirty markers.
    // ------------------------------------------------------------------
    for (const auto& entry : entries) {
        world.removeComponent<TransformTreeChanged>(entry.e);
    }
}

} // namespace Entelechy
