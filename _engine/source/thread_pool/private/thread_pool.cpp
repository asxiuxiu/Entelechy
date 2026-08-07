#include "thread_pool/thread_pool.h"
#include "core/allocator/allocator.h"
#include <memory>

namespace Entelechy
{

// ---------- WorkStealingQueue ----------

WorkStealingQueue::WorkStealingQueue(usize capacity)
{
    usize cap = 1;
    while (cap < capacity)
        cap <<= 1;
    m_capacity = cap;
    m_mask = cap - 1;
    void *mem = DefaultAllocator::alloc(cap * sizeof(std::function<void()>), alignof(std::function<void()>));
    m_buffer = static_cast<std::function<void()> *>(mem);
    for (usize i = 0; i < cap; ++i)
    {
        new (&m_buffer[i]) std::function<void()>();
    }
}

WorkStealingQueue::~WorkStealingQueue()
{
    for (usize i = 0; i < m_capacity; ++i)
    {
        m_buffer[i].~function();
    }
    DefaultAllocator::free(m_buffer);
}

bool WorkStealingQueue::push(std::function<void()> task)
{
    usize b = m_bottom.load(std::memory_order_relaxed);
    usize t = m_top.load(std::memory_order_acquire);

    if (b - t >= m_capacity)
    {
        return false;
    }

    m_buffer[b & m_mask] = std::move(task);
    std::atomic_thread_fence(std::memory_order_release);
    m_bottom.store(b + 1, std::memory_order_relaxed);
    return true;
}

std::function<void()> WorkStealingQueue::pop()
{
    usize b = m_bottom.load(std::memory_order_relaxed);
    usize t = m_top.load(std::memory_order_relaxed);
    if (t >= b)
    {
        return nullptr;
    }

    b = b - 1;
    m_bottom.store(b, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    t = m_top.load(std::memory_order_relaxed);
    if (t <= b)
    {
        // Non-empty after the fence: either multiple elements or we won
        // the race against steal() for the last element.
        if (t == b)
        {
            // Exactly one element — compete with steal().
            if (!m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
            {
                // steal() already took it (or another pop).
                m_bottom.store(b + 1, std::memory_order_relaxed);
                return nullptr;
            }
            m_bottom.store(b + 1, std::memory_order_relaxed);
        }
        return std::move(m_buffer[b & m_mask]);
    }
    else
    {
        // t > b: steal() already took the last element between our first
        // check and the fence. Restore bottom and report empty.
        m_bottom.store(b + 1, std::memory_order_relaxed);
        return nullptr;
    }
}

std::function<void()> WorkStealingQueue::steal()
{
    usize t = m_top.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    usize b = m_bottom.load(std::memory_order_acquire);

    if (t < b)
    {
        // Win the CAS before moving from buffer. If CAS fails, another
        // thread (pop or another steal) already took this element.
        if (m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
        {
            return std::move(m_buffer[t & m_mask]);
        }
    }
    return nullptr;
}

// ---------- ThreadPool ----------

ThreadPool::ThreadPool(usize numThreads) : m_num_threads(numThreads)
{
    // Two-phase construction: allocate and register ALL workers before
    // starting any thread. Worker threads iterate m_workers when stealing —
    // starting threads while pushBack may still reallocate the array is a
    // use-after-free race on the old buffer.
    for (usize i = 0; i < numThreads; ++i)
    {
        void *mem = DefaultAllocator::alloc(sizeof(Worker), alignof(Worker));
        Worker *w = new (mem) Worker();
        m_workers.pushBack(w);
    }
    for (usize i = 0; i < numThreads; ++i)
    {
        m_workers[i]->thread = std::thread([this, w = m_workers[i]]() { runWorkerLoop(w); });
    }
}

ThreadPool::~ThreadPool()
{
    m_stop.store(true, std::memory_order_release);

    for (usize i = 0; i < m_workers.size(); ++i)
    {
        if (m_workers[i]->thread.joinable())
        {
            m_workers[i]->thread.join();
        }
    }

    for (usize i = 0; i < m_workers.size(); ++i)
    {
        m_workers[i]->~Worker();
        DefaultAllocator::free(m_workers[i]);
    }
    m_workers.clear();
}

void ThreadPool::submit(std::function<void()> task)
{
    if (!task)
        return;

    m_pending_tasks.fetch_add(1, std::memory_order_relaxed);

    auto wrapped = [this, t = std::move(task)]() mutable
    {
        t();
        m_pending_tasks.fetch_sub(1, std::memory_order_release);
    };

    // External submissions go to the mutex-guarded global queue.
    //
    // WorkStealingQueue::push is documented as owner-thread-only: the worker
    // owning that queue concurrently runs pop(), and push-vs-pop both do
    // unsynchronized load/store on m_bottom — a foreign push can regress
    // m_bottom, silently dropping freshly pushed tasks and racing
    // std::function writes (observed 2026-08-08 as intermittent lost tasks +
    // bad_function_call crashes in the ThreadPool tests). Correct foreign
    // submission into a Chase-Lev deque needs an MPSC-safe push; until a
    // caller actually needs it (worker-local task spawning), all external
    // tasks flow through the global queue.
    std::lock_guard<std::mutex> lock(m_overflow_mutex);
    m_overflow_tasks.push(std::move(wrapped));
}

void ThreadPool::waitForAll()
{
    while (m_pending_tasks.load(std::memory_order_acquire) > 0)
    {
        std::this_thread::yield();
    }
}

void ThreadPool::runWorkerLoop(Worker *self)
{
    while (!m_stop.load(std::memory_order_acquire))
    {
        std::function<void()> task = self->queue.pop();
        if (task)
        {
            task();
            continue;
        }

        bool stolen = false;
        for (usize i = 0; i < m_workers.size(); ++i)
        {
            if (m_workers[i] == self)
                continue;
            task = m_workers[i]->queue.steal();
            if (task)
            {
                task();
                stolen = true;
                break;
            }
        }
        if (stolen)
            continue;

        {
            std::lock_guard<std::mutex> lock(m_overflow_mutex);
            m_overflow_tasks.pop(task);
        }
        if (task)
        {
            task();
            continue;
        }

        std::this_thread::yield();
    }
}

} // namespace Entelechy
