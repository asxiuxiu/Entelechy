#pragma once
#include "ecs/world/world.h"

namespace Entelechy
{

class ThreadPool;

// QueueDrawsSystem — generates PhaseItems from ViewVisibleList.
// Opaque/AlphaMask go to BinnedRenderPhase; Transparent/UI go to SortedRenderPhase.
//
// Phase 1: single-threaded fallback for small visible lists.
// Phase 2: when a ThreadPool is supplied and the visible count exceeds a
// threshold, items are generated in parallel into thread-local buffers and
// merged deterministically in batch order before binning/sorting.
class QueueDrawsSystem
{
public:
    QueueDrawsSystem() = default;
    explicit QueueDrawsSystem(ThreadPool *threadPool);

    void setThreadPool(ThreadPool *threadPool)
    {
        m_thread_pool = threadPool;
    }

    void run(World &renderWorld);

private:
    ThreadPool *m_thread_pool = nullptr;
};

} // namespace Entelechy
