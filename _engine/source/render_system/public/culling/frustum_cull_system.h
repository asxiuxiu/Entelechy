#pragma once
#include "ecs/world/world.h"

namespace Entelechy
{

class ThreadPool;

// FrustumCullSystem — per-entity frustum culling against the ExtractedView.
// Entities without a RenderAABB are always visible (e.g. skyboxes).
//
// Single-threaded fallback for small scenes when no ThreadPool is supplied.
// With a ThreadPool, once the entity count exceeds a threshold the visible
// list is built in parallel using thread-local buffers and merged
// deterministically in batch order.
class FrustumCullSystem
{
public:
    FrustumCullSystem() = default;
    explicit FrustumCullSystem(ThreadPool *threadPool);

    void setThreadPool(ThreadPool *threadPool)
    {
        m_thread_pool = threadPool;
    }

    void run(World &renderWorld);

private:
    ThreadPool *m_thread_pool = nullptr;
};

} // namespace Entelechy
