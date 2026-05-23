#include "transform_propagation_system.h"
#include "query.h"
#include "transform_component.h"
#include "mat4.h"
#include "hierarchy.h"
#include "thread_pool/thread_pool.h"

namespace Entelechy {

void TransformPropagationSystem::tick(World& world, FrameArena& arena, f32 dt) {
    (void)arena;
    (void)dt;

    // ------------------------------------------------------------------
    // Step 1: Collect all entities with Transform + GlobalTransform
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
        entries.pushBack({e, trans, global, world.getRank(e)});
    }

    if (entries.empty()) return;

    // ------------------------------------------------------------------
    // Step 2: Build rank buckets (O(n) instead of O(n log n) sort)
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
    // Step 3: Propagate local → world by rank, parents before children.
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

            trans->dirty = 0;
        };

        if (m_threadPool && bucket.size() > 16) {
            m_threadPool->parallelFor(bucket.size(), 16, [&](usize i) {
                propagateOne(bucket[i]);
            });
        } else {
            for (Entity e : bucket) {
                propagateOne(e);
            }
        }
    }
}

} // namespace Entelechy
