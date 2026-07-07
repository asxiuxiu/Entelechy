#include "render/queue/QueueDrawsSystem.h"
#include "render/components/RenderComponents.h"
#include "render/components/RenderCamera.h"
#include "render/culling/ViewVisibleList.h"
#include "render/phase/RenderResources.h"
#include "render/queue/PhaseItem.h"
#include "ecs/query/query.h"
#include "thread_pool/thread_pool.h"
#include "core/math/vec.h"
#include <algorithm>
#include <atomic>
#include <thread>

namespace Entelechy
{

namespace
{

constexpr usize kParallelThreshold = 256;

struct LocalPhaseItems
{
    DynamicArray<PhaseItem> opaque;
    DynamicArray<PhaseItem> alpha_mask;
    DynamicArray<PhaseItem> transparent;
    DynamicArray<PhaseItem> ui;
};

bool tryBuildPhaseItem(Entity entity, const World &renderWorld, const ExtractedView &view, PhaseItem &outItem,
                       RenderPhase &outPhase)
{
    const RenderTransform *transform = renderWorld.getComponent<RenderTransform>(entity);
    const RenderMaterial *material = renderWorld.getComponent<RenderMaterial>(entity);
    if (!transform || !material)
        return false;

    // Compute depth in view space (positive = in front of camera).
    Vec3 worldPos = transform->world_matrix.transformPoint(Vec3{0.0f, 0.0f, 0.0f});
    Vec3 viewPos = view.view_matrix.transformPoint(worldPos);
    f32 depth = viewPos.z;

    // Normalize view-space depth to [0, 1] over the view frustum and encode
    // it as a monotonic uint. This avoids the IEEE-754 uint ordering bug for
    // negative or near-zero viewZ values.
    u32 depthBits = encodeLinearDepth(depth, view.near_plane, view.far_plane);

    RenderPhase phase = material->render_phase;
    SortKey key{};
    key.packed.material_id = static_cast<u16>(material->material_asset_id & 0xFFFFu);
    key.packed.phase = static_cast<u8>(phase);
    key.packed.reserved = 0;

    switch (phase)
    {
    case RenderPhase::ShadowMap:
        // Not handled in this simplified checkpoint.
        return false;
    case RenderPhase::Opaque3D:
    case RenderPhase::AlphaMask:
        key.packed.depth = depthBits; // ascending
        break;
    case RenderPhase::Transparent3D:
    case RenderPhase::UI:
        key.packed.depth = ~depthBits; // descending (invert for sort ascending)
        break;
    }

    outItem.render_entity = entity;
    outItem.instance_count = 1;
    outItem.sort_key = key;
    outPhase = phase;
    return true;
}

void pushToPhase(LocalPhaseItems &local, const PhaseItem &item, RenderPhase phase)
{
    switch (phase)
    {
    case RenderPhase::Opaque3D:
        local.opaque.pushBack(item);
        break;
    case RenderPhase::AlphaMask:
        local.alpha_mask.pushBack(item);
        break;
    case RenderPhase::Transparent3D:
        local.transparent.pushBack(item);
        break;
    case RenderPhase::UI:
        local.ui.pushBack(item);
        break;
    default:
        break;
    }
}

void mergeLocalItems(const LocalPhaseItems &local, ViewBinnedPhases *binned, ViewSortedPhases *sorted)
{
    for (const auto &item : local.opaque)
        binned->opaque.addItem(item);
    for (const auto &item : local.alpha_mask)
        binned->alpha_mask.addItem(item);
    for (const auto &item : local.transparent)
        sorted->transparent.addItem(item);
    for (const auto &item : local.ui)
        sorted->ui.addItem(item);
}

} // namespace

QueueDrawsSystem::QueueDrawsSystem(ThreadPool *threadPool) : m_thread_pool(threadPool) {}

void QueueDrawsSystem::run(World &renderWorld)
{
    // Locate the single view entity (for depth calculation and resource lookup).
    Entity viewEntity{0, 0};
    const ExtractedView *view = nullptr;
    ConstQuery<ExtractedView> viewQuery(renderWorld);
    for (auto [ve, ev] : viewQuery)
    {
        viewEntity = ve;
        view = ev;
        break;
    }
    if (!view)
        return;

    // Ensure phase containers are attached to the same view entity.
    ViewBinnedPhases *binned = renderWorld.getComponent<ViewBinnedPhases>(viewEntity);
    if (!binned)
    {
        renderWorld.addComponent(viewEntity, ViewBinnedPhases{});
        binned = renderWorld.getComponent<ViewBinnedPhases>(viewEntity);
    }
    binned->opaque.clear();
    binned->alpha_mask.clear();

    ViewSortedPhases *sorted = renderWorld.getComponent<ViewSortedPhases>(viewEntity);
    if (!sorted)
    {
        renderWorld.addComponent(viewEntity, ViewSortedPhases{});
        sorted = renderWorld.getComponent<ViewSortedPhases>(viewEntity);
    }
    sorted->transparent.clear();
    sorted->ui.clear();

    // Get the visible list from the same view entity.
    const ViewVisibleList *visibleList = renderWorld.getComponent<ViewVisibleList>(viewEntity);
    if (!visibleList)
        return;

    const usize total = visibleList->entities.size();

    if (!m_thread_pool || m_thread_pool->workerCount() == 0 || total < kParallelThreshold)
    {
        LocalPhaseItems local;
        for (usize i = 0; i < total; ++i)
        {
            Entity entity = visibleList->entities[i];
            PhaseItem item;
            RenderPhase phase;
            if (!tryBuildPhaseItem(entity, renderWorld, *view, item, phase))
                continue;
            pushToPhase(local, item, phase);
        }
        mergeLocalItems(local, binned, sorted);
        sorted->transparent.prepare();
        sorted->ui.prepare();
        return;
    }

    // Parallel path: split the visible list into batches.
    // Each worker writes PhaseItems into thread-local buffers; results are
    // merged in batch order so the final bin/sort order remains deterministic.
    const usize numWorkers = m_thread_pool->workerCount();
    const usize desiredBatches = numWorkers * 4;
    const usize batchSize = (total + desiredBatches - 1) / desiredBatches;
    const usize numBatches = (total + batchSize - 1) / batchSize;

    DynamicArray<LocalPhaseItems> chunkBuffers;
    chunkBuffers.resize(numBatches);

    std::atomic<usize> nextBatch{0};
    std::atomic<usize> completedTasks{0};

    for (usize w = 0; w < numWorkers; ++w)
    {
        m_thread_pool->submit(
            [&, total]()
            {
                while (true)
                {
                    usize batch = nextBatch.fetch_add(1, std::memory_order_relaxed);
                    if (batch >= numBatches)
                        break;

                    usize start = batch * batchSize;
                    usize end = std::min(start + batchSize, total);

                    auto &local = chunkBuffers[batch];
                    for (usize i = start; i < end; ++i)
                    {
                        Entity entity = visibleList->entities[i];
                        PhaseItem item;
                        RenderPhase phase;
                        if (!tryBuildPhaseItem(entity, renderWorld, *view, item, phase))
                            continue;
                        pushToPhase(local, item, phase);
                    }
                }
                completedTasks.fetch_add(1, std::memory_order_release);
            });
    }

    while (completedTasks.load(std::memory_order_acquire) < numWorkers)
    {
        std::this_thread::yield();
    }

    for (usize b = 0; b < numBatches; ++b)
    {
        mergeLocalItems(chunkBuffers[b], binned, sorted);
    }

    sorted->transparent.prepare();
    sorted->ui.prepare();
}

} // namespace Entelechy
