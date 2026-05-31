#pragma once
#include "core/foundation_types.h"
#include "core/container/dynamic_array.h"
#include <atomic>
#include <thread>
#include <functional>
#include <mutex>

namespace Entelechy {

// Simple FIFO overflow queue based on DynamicArray.
// Replaces std::deque to avoid bypassing the engine allocator system.
class OverflowQueue {
public:
    void push(std::function<void()> task) {
        m_tasks.pushBack(std::move(task));
    }

    bool pop(std::function<void()>& out) {
        if (m_head >= m_tasks.size()) return false;
        out = std::move(m_tasks[m_head]);
        ++m_head;
        // Compact if we've consumed more than half the array
        if (m_head > m_tasks.size() / 2) {
            compact();
        }
        return true;
    }

    bool empty() const {
        return m_head >= m_tasks.size();
    }

    void clear() {
        m_tasks.clear();
        m_head = 0;
    }

private:
    void compact() {
        usize newSize = m_tasks.size() - m_head;
        for (usize i = 0; i < newSize; ++i) {
            m_tasks[i] = std::move(m_tasks[m_head + i]);
        }
        m_tasks.resize(newSize);
        m_head = 0;
    }

    DynamicArray<std::function<void()>> m_tasks;
    usize m_head = 0;
};

// Fixed-capacity Chase-Lev Work-Stealing Deque.
// No dynamic resizing — this avoids the use-after-free race condition
// that occurs when grow() runs concurrently with steal().
// When full, tasks fall back to the ThreadPool's global overflow queue.
class WorkStealingQueue {
public:
    explicit WorkStealingQueue(usize capacity = 4096);
    ~WorkStealingQueue();

    // Called only by the owner thread.
    // Returns false if the queue is full.
    bool push(std::function<void()> task);

    // Called only by the owner thread.
    std::function<void()> pop();

    // Called by other worker threads.
    std::function<void()> steal();

    [[nodiscard]] usize capacity() const { return m_capacity; }

private:
    usize m_capacity;
    usize m_mask;
    std::function<void()>* m_buffer;

    std::atomic<usize> m_top{0};
    std::atomic<usize> m_bottom{0};
};

class ThreadPool {
public:
    explicit ThreadPool(usize numThreads);
    ~ThreadPool();

    void submit(std::function<void()> task);

    // Blocks until all currently submitted tasks complete.
    void waitForAll();

    [[nodiscard]] usize workerCount() const { return m_num_threads; }

    // Dynamic-index parallel for. Returns when all iterations complete.
    template<typename Func>
    void parallelFor(usize count, usize minBatchSize, Func&& fn);

private:
    struct Worker {
        WorkStealingQueue queue;
        std::thread thread;
    };

    void runWorkerLoop(Worker* self);

    DynamicArray<Worker*> m_workers;
    std::atomic<bool> m_stop{false};
    std::atomic<usize> m_pending_tasks{0};
    usize m_num_threads = 0;

    // Thread-safe overflow for when a local queue is full.
    std::mutex m_overflow_mutex;
    OverflowQueue m_overflow_tasks;
};

// ---------- Template implementation ----------

template<typename Func>
void ThreadPool::parallelFor(usize count, usize minBatchSize, Func&& fn) {
    if (m_num_threads == 0 || count <= minBatchSize) {
        for (usize i = 0; i < count; ++i) {
            fn(i);
        }
        return;
    }

    usize numBatches = (count + minBatchSize - 1) / minBatchSize;
    if (numBatches > m_num_threads * 4) {
        numBatches = m_num_threads * 4;
    }
    usize batchSize = count / numBatches;
    if (batchSize < 1) batchSize = 1;

    std::atomic<usize> nextIndex{0};
    std::atomic<usize> completed{0};

    for (usize b = 0; b < numBatches; ++b) {
        submit([&nextIndex, &completed, count, batchSize, &fn]() {
            while (true) {
                usize start = nextIndex.fetch_add(batchSize, std::memory_order_relaxed);
                if (start >= count) break;
                usize end = start + batchSize;
                if (end > count) end = count;
                for (usize i = start; i < end; ++i) {
                    fn(i);
                }
            }
            completed.fetch_add(1, std::memory_order_release);
        });
    }

    while (completed.load(std::memory_order_acquire) < numBatches) {
        std::this_thread::yield();
    }
}

} // namespace Entelechy
