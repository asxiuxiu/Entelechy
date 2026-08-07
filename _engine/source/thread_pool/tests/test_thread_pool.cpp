#include "test/test_framework.h"
#include "thread_pool/thread_pool.h"
#include <atomic>

// Regression stress test (2026-08-08): Submit1000Tasks intermittently hung or
// crashed (~25% of runs) because external submit() pushed directly into a
// worker's Chase-Lev queue, racing the owner thread's pop() on m_bottom.
// Fixed by routing all external submissions through the global mutex queue.
// This loop runs many submit/wait rounds with per-task execution tracking
// and a bounded wait so any recurrence shows up as a reported failure mode
// (lost task vs double execution) instead of hanging the whole suite.
TEST(ThreadPool, StressDiagnostic)
{
    constexpr u32 kTasks = 1000;
    for (u32 round = 0; round < 300; ++round)
    {
        Entelechy::ThreadPool pool(4);
        std::atomic<u32> counter{0};
        auto *execCount = new std::atomic<u32>[kTasks];
        for (u32 i = 0; i < kTasks; ++i)
            execCount[i].store(0, std::memory_order_relaxed);

        for (u32 i = 0; i < kTasks; ++i)
        {
            pool.submit([&counter, execCount, i]()
                        {
                            execCount[i].fetch_add(1, std::memory_order_relaxed);
                            counter.fetch_add(1, std::memory_order_relaxed);
                        });
        }

        // Bounded wait (~10 s worst case) instead of waitForAll().
        bool done = false;
        for (u32 spins = 0; spins < 10000000; ++spins)
        {
            if (counter.load(std::memory_order_acquire) >= kTasks)
            {
                done = true;
                break;
            }
            std::this_thread::yield();
        }

        u32 lost = 0;
        u32 doubleExec = 0;
        for (u32 i = 0; i < kTasks; ++i)
        {
            const u32 n = execCount[i].load(std::memory_order_relaxed);
            if (n == 0)
                ++lost;
            else if (n > 1)
                ++doubleExec;
        }
        if (!done || lost > 0 || doubleExec > 0)
        {
            printf("    [STRESS] round %u: done=%d counter=%u lost=%u doubleExec=%u\n", round, done ? 1 : 0,
                   counter.load(), lost, doubleExec);
        }
        delete[] execCount;
        // Fail the test on any anomaly.
        ASSERT_TRUE(done);
        ASSERT_EQ(lost, 0u);
        ASSERT_EQ(doubleExec, 0u);
    }
}

TEST(ThreadPool, Submit1000Tasks)
{
    Entelechy::ThreadPool pool(4);
    std::atomic<u32> counter{0};

    for (u32 i = 0; i < 1000; ++i)
    {
        pool.submit([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });
    }

    pool.waitForAll();
    ASSERT_EQ(counter.load(), 1000u);
}

TEST(ThreadPool, ParallelForCorrectness)
{
    Entelechy::ThreadPool pool(4);
    constexpr usize N = 10000;
    u32 values[N] = {};

    pool.parallelFor(N, 64, [&values](usize i) { values[i] = static_cast<u32>(i * i); });

    for (usize i = 0; i < N; ++i)
    {
        ASSERT_EQ(values[i], static_cast<u32>(i * i));
    }
}
