#include "render/culling/FrustumCullSystem.h"
#include "render/components/RenderCamera.h"
#include "render/components/RenderComponents.h"
#include "core/math/aabb.h"
#include "render/culling/ViewVisibleList.h"
#include "ecs/component/component_array.h"
#include "ecs/query/query.h"
#include "thread_pool/thread_pool.h"
#include <algorithm>
#include <atomic>
#include <thread>

namespace Entelechy {

namespace {

constexpr usize kParallelThreshold = 256;

bool isEntityVisible(const World& world, Entity entity, const ExtractedView& view) {
    const AABB* aabb = world.getComponent<AABB>(entity);
    if (aabb) {
        return view.frustum.intersectsAABB(*aabb);
    }
    return true;
}

} // namespace

FrustumCullSystem::FrustumCullSystem(ThreadPool* threadPool)
    : m_thread_pool(threadPool) {
}

void FrustumCullSystem::run(World& renderWorld) {
    // Find the single view entity.
    Entity viewEntity{0, 0};
    const ExtractedView* view = nullptr;
    Query<ExtractedView> viewQuery(renderWorld);
    for (auto [ve, ev] : viewQuery) {
        viewEntity = ve;
        view = ev;
        break;
    }
    if (!view) return;

    // Ensure ViewVisibleList is attached to the same view entity.
    ViewVisibleList* visibleList = renderWorld.getComponent<ViewVisibleList>(viewEntity);
    if (!visibleList) {
        renderWorld.addComponent(viewEntity, ViewVisibleList{});
        visibleList = renderWorld.getComponent<ViewVisibleList>(viewEntity);
    }
    visibleList->entities.clear();

    const ComponentArray<RenderTransform>* transformArray =
        renderWorld.getComponentArray<RenderTransform>();
    if (!transformArray || transformArray->count() == 0) return;

    const usize total = transformArray->count();
    const u32* ids = transformArray->entityIds();
    (void)ids;

    if (!m_thread_pool || m_thread_pool->workerCount() == 0 || total < kParallelThreshold) {
        for (usize i = 0; i < total; ++i) {
            Entity entity{ids[i], renderWorld.getEntityGeneration(ids[i])};
            if (isEntityVisible(renderWorld, entity, *view)) {
                visibleList->entities.pushBack(entity);
            }
        }
        return;
    }

    // Parallel path: split the dense RenderTransform array into batches.
    // Each worker writes to its own DynamicArray; results are merged in batch
    // order so output remains deterministic.
    const usize numWorkers = m_thread_pool->workerCount();
    const usize desiredBatches = numWorkers * 4;
    const usize batchSize = (total + desiredBatches - 1) / desiredBatches;
    const usize numBatches = (total + batchSize - 1) / batchSize;

    DynamicArray<DynamicArray<Entity>> chunkLists;
    chunkLists.resize(numBatches);

    std::atomic<usize> nextBatch{0};
    std::atomic<usize> completedTasks{0};

    for (usize w = 0; w < numWorkers; ++w) {
        m_thread_pool->submit([&, total]() {
            while (true) {
                usize batch = nextBatch.fetch_add(1, std::memory_order_relaxed);
                if (batch >= numBatches) break;

                usize start = batch * batchSize;
                usize end = std::min(start + batchSize, total);

                auto& local = chunkLists[batch];
                for (usize i = start; i < end; ++i) {
                    Entity entity{ids[i], renderWorld.getEntityGeneration(ids[i])};
                    if (isEntityVisible(renderWorld, entity, *view)) {
                        local.pushBack(entity);
                    }
                }
            }
            completedTasks.fetch_add(1, std::memory_order_release);
        });
    }

    while (completedTasks.load(std::memory_order_acquire) < numWorkers) {
        std::this_thread::yield();
    }

    for (usize b = 0; b < numBatches; ++b) {
        for (Entity entity : chunkLists[b]) {
            visibleList->entities.pushBack(entity);
        }
    }
}

} // namespace Entelechy
