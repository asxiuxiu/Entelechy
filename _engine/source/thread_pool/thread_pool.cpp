#include "thread_pool.h"

namespace Entelechy {

// ---------- WorkStealingQueue ----------

WorkStealingQueue::WorkStealingQueue(usize capacity) {
    usize cap = 1;
    while (cap < capacity) cap <<= 1;
    m_capacity = cap;
    m_mask = cap - 1;
    m_buffer = new std::function<void()>[cap];
}

WorkStealingQueue::~WorkStealingQueue() {
    delete[] m_buffer;
}

bool WorkStealingQueue::push(std::function<void()> task) {
    usize b = m_bottom.load(std::memory_order_relaxed);
    usize t = m_top.load(std::memory_order_acquire);

    if (b - t >= m_capacity) {
        return false;
    }

    m_buffer[b & m_mask] = std::move(task);
    std::atomic_thread_fence(std::memory_order_release);
    m_bottom.store(b + 1, std::memory_order_relaxed);
    return true;
}

std::function<void()> WorkStealingQueue::pop() {
    usize b = m_bottom.load(std::memory_order_relaxed);
    usize t = m_top.load(std::memory_order_relaxed);
    if (t >= b) {
        return nullptr;
    }

    b = b - 1;
    m_bottom.store(b, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    t = m_top.load(std::memory_order_relaxed);
    if (t <= b) {
        // Non-empty after the fence: either multiple elements or we won
        // the race against steal() for the last element.
        if (t == b) {
            // Exactly one element — compete with steal().
            if (!m_top.compare_exchange_strong(t, t + 1,
                    std::memory_order_seq_cst, std::memory_order_relaxed)) {
                // steal() already took it (or another pop).
                m_bottom.store(b + 1, std::memory_order_relaxed);
                return nullptr;
            }
            m_bottom.store(b + 1, std::memory_order_relaxed);
        }
        return std::move(m_buffer[b & m_mask]);
    } else {
        // t > b: steal() already took the last element between our first
        // check and the fence. Restore bottom and report empty.
        m_bottom.store(b + 1, std::memory_order_relaxed);
        return nullptr;
    }
}

std::function<void()> WorkStealingQueue::steal() {
    usize t = m_top.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    usize b = m_bottom.load(std::memory_order_acquire);

    if (t < b) {
        // Win the CAS before moving from buffer. If CAS fails, another
        // thread (pop or another steal) already took this element.
        if (m_top.compare_exchange_strong(t, t + 1,
                std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return std::move(m_buffer[t & m_mask]);
        }
    }
    return nullptr;
}

// ---------- ThreadPool ----------

ThreadPool::ThreadPool(usize numThreads) : m_num_threads(numThreads) {
    for (usize i = 0; i < numThreads; ++i) {
        Worker* w = new Worker();
        w->thread = std::thread([this, w]() {
            runWorkerLoop(w);
        });
        m_workers.pushBack(w);
    }
}

ThreadPool::~ThreadPool() {
    m_stop.store(true, std::memory_order_release);

    for (usize i = 0; i < m_workers.size(); ++i) {
        if (m_workers[i]->thread.joinable()) {
            m_workers[i]->thread.join();
        }
    }

    for (usize i = 0; i < m_workers.size(); ++i) {
        delete m_workers[i];
    }
    m_workers.clear();
}

void ThreadPool::submit(std::function<void()> task) {
    if (!task) return;

    m_pending_tasks.fetch_add(1, std::memory_order_relaxed);

    auto wrapped = [this, t = std::move(task)]() mutable {
        t();
        m_pending_tasks.fetch_sub(1, std::memory_order_release);
    };

    static std::atomic<usize> s_next{0};
    usize idx = s_next.fetch_add(1, std::memory_order_relaxed) % m_workers.size();

    if (!m_workers[idx]->queue.push(std::move(wrapped))) {
        std::lock_guard<std::mutex> lock(m_overflow_mutex);
        m_overflow_tasks.push_back(std::move(wrapped));
    }
}

void ThreadPool::waitForAll() {
    while (m_pending_tasks.load(std::memory_order_acquire) > 0) {
        std::this_thread::yield();
    }
}

void ThreadPool::runWorkerLoop(Worker* self) {
    while (!m_stop.load(std::memory_order_acquire)) {
        std::function<void()> task = self->queue.pop();
        if (task) {
            task();
            continue;
        }

        bool stolen = false;
        for (usize i = 0; i < m_workers.size(); ++i) {
            if (m_workers[i] == self) continue;
            task = m_workers[i]->queue.steal();
            if (task) {
                task();
                stolen = true;
                break;
            }
        }
        if (stolen) continue;

        {
            std::lock_guard<std::mutex> lock(m_overflow_mutex);
            if (!m_overflow_tasks.empty()) {
                task = std::move(m_overflow_tasks.front());
                m_overflow_tasks.pop_front();
            }
        }
        if (task) {
            task();
            continue;
        }

        std::this_thread::yield();
    }
}

} // namespace Entelechy
