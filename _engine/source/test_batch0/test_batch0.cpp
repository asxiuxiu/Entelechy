#include <cstdio>
#include <atomic>
#include "thread_pool/thread_pool.h"
#include "vfs/vfs.h"
#include "vfs/mount_point.h"
#include "core/world.h"
#include "core/scheduler.h"
#include "core/command_buffer.h"

static int testBatch0() {
    printf("=== Batch 0 Acceptance Tests ===\n\n");
    int failures = 0;

    // Test 1: Submit 1000 counter tasks
    printf("[ThreadPool] Test 1: Submit 1000 tasks\n");
    {
        Entelechy::ThreadPool pool(4);
        std::atomic<u32> counter{0};

        for (u32 i = 0; i < 1000; ++i) {
            pool.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }

        pool.waitForAll();
        if (counter.load() != 1000) {
            printf("  FAIL: counter=%u (expected 1000)\n", counter.load());
            failures++;
        } else {
            printf("  PASS: 1000/1000 tasks completed\n");
        }
    }

    // Test 2: parallelFor correctness
    printf("[ThreadPool] Test 2: parallelFor correctness\n");
    {
        Entelechy::ThreadPool pool(4);
        constexpr usize N = 10000;
        u32 values[N] = {};

        pool.parallelFor(N, 64, [&values](usize i) {
            values[i] = static_cast<u32>(i * i);
        });

        bool ok = true;
        for (usize i = 0; i < N; ++i) {
            if (values[i] != static_cast<u32>(i * i)) { ok = false; break; }
        }
        if (!ok) { printf("  FAIL\n"); failures++; }
        else     { printf("  PASS: parallelFor %zu elements correct\n", N); }
    }

    // Test 3: VFS FileSystemMountPoint
    printf("\n[VFS] Test 3: FileSystemMountPoint read/write\n");
    {
        Entelechy::VFS vfs;
        auto* fsMount = new Entelechy::FileSystemMountPoint("build/test_vfs");
        vfs.mount("/test", fsMount);

        const char* testData = "Hello from VFS!";
        usize testLen = 17;
        bool writeOk = fsMount->writeFile("hello.txt", reinterpret_cast<const u8*>(testData), testLen);
        printf("  Write: %s\n", writeOk ? "PASS" : "FAIL");
        if (!writeOk) failures++;

        auto file = vfs.readFile(Entelechy::Path("hello.txt"));
        printf("  Read valid: %s\n", file.valid ? "PASS" : "FAIL");
        if (!file.valid) failures++;

        if (file.valid && file.bytes.size() == testLen) {
            bool match = true;
            for (usize i = 0; i < testLen; ++i) {
                if (file.bytes[i] != static_cast<u8>(testData[i])) { match = false; break; }
            }
            printf("  Content match: %s\n", match ? "PASS" : "FAIL");
            if (!match) failures++;
        }
        vfs.clear();
    }

    // Test 4: VFS MemoryMountPoint
    printf("[VFS] Test 4: MemoryMountPoint\n");
    {
        Entelechy::VFS vfs;
        auto* memMount = new Entelechy::MemoryMountPoint();
        const char* memData = "Memory file content";
        memMount->registerFile("mem/test.txt", reinterpret_cast<const u8*>(memData), 19);
        vfs.mount("/mem", memMount);

        printf("  Exists: %s\n", vfs.exists(Entelechy::Path("mem/test.txt")) ? "PASS" : "FAIL");
        auto file = vfs.readFile(Entelechy::Path("mem/test.txt"));
        printf("  Read valid: %s\n", file.valid ? "PASS" : "FAIL");
        if (!file.valid) failures++;
        vfs.clear();
    }

    printf("\n=== Batch 0 Results: %d failure(s) ===\n", failures);
    return failures;
}

static int testBatchA() {
    printf("\n=== Batch A Acceptance Tests ===\n");
    int failures = 0;

    // Test 5: EntityRegistry + World spawn/destroy
    printf("\n[EntityRegistry] Test 5: Spawn and destroy\n");
    {
        Entelechy::World world;
        Entelechy::Scheduler scheduler;
        world.bindCommandBuffer(&scheduler.commandBuffer());

        auto e1 = world.spawn();
        auto e2 = world.spawn();

        bool ok = world.valid(e1) && world.valid(e2) && e1.id != e2.id;
        printf("  Spawn: %s\n", ok ? "PASS" : "FAIL");
        if (!ok) failures++;

        world.destroy(e1);
        scheduler.tickOnce(world, 0.016f);

        ok = !world.valid(e1) && world.valid(e2);
        printf("  Destroy: %s\n", ok ? "PASS" : "FAIL");
        if (!ok) failures++;
    }

    // Test 6: CommandBuffer destroyWithChildren
    printf("\n[CommandBuffer] Test 6: destroyWithChildren\n");
    {
        Entelechy::World world;
        Entelechy::Scheduler scheduler;
        world.bindCommandBuffer(&scheduler.commandBuffer());

        auto parent = world.spawn();
        auto child = world.spawn();
        auto grandchild = world.spawn();

        world.setParent(child, parent);
        world.setParent(grandchild, child);

        scheduler.tickOnce(world, 0.016f);

        bool ok = world.hasComponent<Entelechy::ChildOf>(child) &&
                  world.hasComponent<Entelechy::ChildOf>(grandchild) &&
                  world.hasComponent<Entelechy::Children>(parent) &&
                  world.hasComponent<Entelechy::Children>(child);
        printf("  Hierarchy setup: %s\n", ok ? "PASS" : "FAIL");
        if (!ok) failures++;

        scheduler.commandBuffer().destroyWithChildren(parent);
        scheduler.tick(world, 0.016f);

        ok = !world.valid(parent) && !world.valid(child) && !world.valid(grandchild);
        printf("  Recursive destroy: %s\n", ok ? "PASS" : "FAIL");
        if (!ok) failures++;
    }

    // Test 7: Batch spawn with parent relationships
    printf("\n[World] Test 7: Batch spawn 50 entities with hierarchy\n");
    {
        Entelechy::World world;
        Entelechy::Scheduler scheduler;
        world.bindCommandBuffer(&scheduler.commandBuffer());

        auto root = world.spawn();
        world.addComponent<Entelechy::Position>(root, {0.0f, 0.0f});

        auto batch = scheduler.commandBuffer().beginBatch();
        auto entities = world.spawnBatch(50);
        for (usize i = 0; i < entities.size(); ++i) {
            world.addComponent<Entelechy::Position>(entities[i], {static_cast<f32>(i), 0.0f});
            world.setParent(entities[i], root);
        }
        scheduler.commandBuffer().endBatch(batch);

        bool beforeTick = world.valid(entities[0]) &&
                          world.hasComponent<Entelechy::Position>(entities[0]) &&
                          !world.hasComponent<Entelechy::Children>(root);
        printf("  Before tick (setParent buffered): %s\n", beforeTick ? "PASS" : "FAIL");
        if (!beforeTick) failures++;

        scheduler.tickOnce(world, 0.016f);

        bool afterTick = world.hasComponent<Entelechy::Children>(root);
        const auto* children = world.getComponent<Entelechy::Children>(root);
        bool countOk = children && children->entities.size() == 50;
        printf("  After tick: %s\n", afterTick ? "PASS" : "FAIL");
        printf("  Child count: %s\n", countOk ? "PASS" : "FAIL");
        if (!afterTick) failures++;
        if (!countOk) failures++;
    }

    printf("\n=== Batch A Results: %d failure(s) ===\n", failures);
    return failures;
}

int main() {
    int failures = 0;
    failures += testBatch0();
    failures += testBatchA();
    printf("\n=== TOTAL: %d failure(s) ===\n", failures);
    return failures;
}
