#include "test/test_framework.h"
#include "thread_pool/thread_pool.h"
#include <atomic>

TEST(ThreadPool, Submit1000Tasks) {
    Entelechy::ThreadPool pool(4);
    std::atomic<u32> counter{0};

    for (u32 i = 0; i < 1000; ++i) {
        pool.submit([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.waitForAll();
    ASSERT_EQ(counter.load(), 1000u);
}

TEST(ThreadPool, ParallelForCorrectness) {
    Entelechy::ThreadPool pool(4);
    constexpr usize N = 10000;
    u32 values[N] = {};

    pool.parallelFor(N, 64, [&values](usize i) {
        values[i] = static_cast<u32>(i * i);
    });

    for (usize i = 0; i < N; ++i) {
        ASSERT_EQ(values[i], static_cast<u32>(i * i));
    }
}
