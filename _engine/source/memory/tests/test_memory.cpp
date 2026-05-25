#include "test_framework.h"
#include "allocator.h"
#include "frame_arena.h"
#include "object_pool.h"
#include <cstring>

using namespace Entelechy;

TEST(Allocator, DefaultAllocAndFree) {
    void* p = DefaultAllocator::alloc(64, 8);
    ASSERT_TRUE(p != nullptr);
    std::memset(p, 0xAB, 64);
    DefaultAllocator::free(p);
}

TEST(Allocator, AlignedAlloc) {
    void* p = DefaultAllocator::alloc(32, 64);
    ASSERT_TRUE(p != nullptr);
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    ASSERT_EQ(addr % 64, 0u);
    DefaultAllocator::free(p);
}

TEST(Allocator, VirtualWrapper) {
    DefaultAllocatorV alloc;
    void* p = alloc.allocate(64, 8);
    ASSERT_TRUE(p != nullptr);
    alloc.free(p);
}

TEST(Allocator, GlobalAllocator) {
    IAllocator* alloc = GetGlobalAllocator();
    ASSERT_TRUE(alloc != nullptr);
    void* p = alloc->allocate(32, 8);
    ASSERT_TRUE(p != nullptr);
    alloc->free(p);
}

TEST(FrameArena, BasicAlloc) {
    FrameArena arena(1024);
    void* p = arena.allocate(100, 8);
    ASSERT_TRUE(p != nullptr);
}

TEST(FrameArena, AlignedAlloc) {
    FrameArena arena(1024);
    void* p = arena.allocate(1, 64);
    ASSERT_TRUE(p != nullptr);
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    ASSERT_EQ(addr % 64, 0u);
}

TEST(FrameArena, CapacityTracking) {
    FrameArena arena(1024);
    ASSERT_EQ(arena.capacity(), 1024u);
    ASSERT_EQ(arena.consumedBytes(), 0u);

    arena.allocate(64, 8);
    ASSERT_EQ(arena.consumedBytes(), 64u);

    arena.allocate(64, 8);
    ASSERT_EQ(arena.consumedBytes(), 128u);
}

TEST(FrameArena, Reset) {
    FrameArena arena(1024);
    void* p1 = arena.allocate(100, 8);
    ASSERT_TRUE(p1 != nullptr);
    arena.reset();
    ASSERT_EQ(arena.consumedBytes(), 0u);

    void* p2 = arena.allocate(100, 8);
    ASSERT_TRUE(p2 != nullptr);
    ASSERT_EQ(p2, p1);
}

TEST(FrameArena, Overflow) {
    FrameArena arena(64);
    void* p1 = arena.allocate(32, 8);
    void* p2 = arena.allocate(32, 8);
    void* p3 = arena.allocate(32, 8);
    ASSERT_TRUE(p1 != nullptr);
    ASSERT_TRUE(p2 != nullptr);
    ASSERT_TRUE(p3 != nullptr);
    ASSERT_TRUE(p3 != p1);
}

TEST(FrameArena, Stats) {
    FrameArena arena(1024);
    arena.allocate(10, 8);
    arena.allocate(20, 8);
    AllocatorStats stats = arena.getStats();
    ASSERT_EQ(stats.allocationCount, 2u);
    ASSERT_EQ(stats.activeCount, 0u);
}

TEST(FrameArena, MemMarkRollback) {
    FrameArena arena(1024);
    arena.allocate(64, 8);
    usize before = arena.consumedBytes();

    {
        MemMark mark(&arena);
        arena.allocate(64, 8);
        ASSERT_EQ(arena.consumedBytes(), before + 64);
        mark.rollback();
    }

    ASSERT_EQ(arena.consumedBytes(), before);
}

TEST(FrameArena, MemMarkAutoRollback) {
    FrameArena arena(1024);
    arena.allocate(50, 8);
    usize before = arena.consumedBytes();

    {
        MemMark mark(&arena);
        arena.allocate(100, 8);
    }

    ASSERT_EQ(arena.consumedBytes(), before);
}

TEST(FrameArena, MemMarkIdempotent) {
    FrameArena arena(1024);
    arena.allocate(50, 8);
    usize before = arena.consumedBytes();

    MemMark mark(&arena);
    arena.allocate(100, 8);
    mark.rollback();
    mark.rollback();
    ASSERT_EQ(arena.consumedBytes(), before);
}

TEST(FrameArena, Swap) {
    FrameArena a(1024);
    FrameArena b(2048);

    a.allocate(100, 8);
    b.allocate(200, 8);

    usize aBefore = a.capacity();
    usize bBefore = b.capacity();

    a.swap(b);

    ASSERT_EQ(a.capacity(), bBefore);
    ASSERT_EQ(b.capacity(), aBefore);
}

TEST(FrameArena, FreeNoOp) {
    FrameArena arena(1024);
    void* p = arena.allocate(100, 8);
    arena.free(p);
    ASSERT_EQ(arena.consumedBytes(), 100u);
}

TEST(FrameArenaRing, Basic) {
    FrameArenaRing<2> ring(1024);
    FrameArena& a = ring.current();
    void* p = a.allocate(100, 8);
    ASSERT_TRUE(p != nullptr);

    ring.advance();
    FrameArena& b = ring.current();
    ASSERT_TRUE(&b != &a);

    ring.resetAll();
    ASSERT_EQ(a.consumedBytes(), 0u);
    ASSERT_EQ(b.consumedBytes(), 0u);
}

TEST(PoolHandle, DefaultInvalid) {
    PoolHandle h;
    ASSERT_FALSE(h.valid());
}

TEST(PoolHandle, Valid) {
    PoolHandle h{0, 1};
    ASSERT_TRUE(h.valid());
}

TEST(PoolHandle, Equality) {
    PoolHandle a{0, 1};
    PoolHandle b{0, 1};
    PoolHandle c{1, 1};
    ASSERT_TRUE(a == b);
    ASSERT_TRUE(a != c);
}

TEST(ObjectPool, AllocFree) {
    ObjectPool<int> pool;
    PoolHandle h = pool.alloc(42);
    ASSERT_TRUE(h.valid());
    ASSERT_TRUE(pool.isValid(h));
    ASSERT_EQ(*pool.resolve(h), 42);

    pool.free(h);
    ASSERT_FALSE(pool.isValid(h));
}

TEST(ObjectPool, GenerationSafety) {
    ObjectPool<int> pool;
    PoolHandle h1 = pool.alloc(10);
    pool.free(h1);

    PoolHandle h2 = pool.alloc(20);
    ASSERT_TRUE(h2.valid());
    ASSERT_FALSE(pool.isValid(h1));
    ASSERT_TRUE(pool.isValid(h2));
}

TEST(ObjectPool, MultipleObjects) {
    ObjectPool<int> pool;
    PoolHandle h1 = pool.alloc(1);
    PoolHandle h2 = pool.alloc(2);
    PoolHandle h3 = pool.alloc(3);

    ASSERT_EQ(*pool.resolve(h1), 1);
    ASSERT_EQ(*pool.resolve(h2), 2);
    ASSERT_EQ(*pool.resolve(h3), 3);

    ASSERT_EQ(pool.count(), 3u);
    ASSERT_FALSE(pool.empty());
}

TEST(ObjectPool, DynamicGrowth) {
    ObjectPool<int> pool(1);
    ASSERT_EQ(pool.capacity(), 1024u);

    for (int i = 0; i < 1025; ++i) {
        (void)pool.alloc(i);
    }
    ASSERT_EQ(pool.capacity(), 2048u);
    ASSERT_EQ(pool.count(), 1025u);
}

TEST(ObjectPool, FreeByPointer) {
    ObjectPool<int> pool;
    PoolHandle h = pool.alloc(99);
    int* p = pool.resolve(h);
    ASSERT_TRUE(p != nullptr);

    pool.free(p);
    ASSERT_FALSE(pool.isValid(h));
}

TEST(ObjectPool, IAllocatorInterface) {
    ObjectPool<int> pool;
    void* p = pool.allocate(sizeof(int), 8);
    ASSERT_TRUE(p != nullptr);

    int* ip = static_cast<int*>(p);
    *ip = 123;
    ASSERT_EQ(*ip, 123);

    pool.free(p);
}

TEST(ObjectPool, IAllocatorLargeRequest) {
    ObjectPool<int> pool;
    void* p = pool.allocate(sizeof(int) + 1, 8);
    ASSERT_TRUE(p == nullptr);
}

TEST(ObjectPool, Stats) {
    ObjectPool<int> pool;
    (void)pool.alloc(1);
    (void)pool.alloc(2);
    (void)pool.alloc(3);

    AllocatorStats stats = pool.getStats();
    ASSERT_EQ(stats.allocationCount, 3u);
    ASSERT_EQ(stats.activeCount, 3u);
    ASSERT_EQ(stats.totalAllocated, 3u * sizeof(int));
}

TEST(ObjectPool, InitialBlockCountZero) {
    ObjectPool<int> pool(0);
    ASSERT_EQ(pool.capacity(), 1024u);
    PoolHandle h = pool.alloc(42);
    ASSERT_TRUE(h.valid());
}
