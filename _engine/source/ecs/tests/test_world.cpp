#include "test/test_framework.h"
#include "ecs/world/world.h"
#include "ecs/world/scheduler.h"
#include "ecs/world/command_buffer.h"

TEST(Core, WorldSpawnAndDestroy) {
    Entelechy::World world;
    Entelechy::Scheduler scheduler;
    world.bindCommandBuffer(&scheduler.commandBuffer());

    auto e1 = world.spawn();
    auto e2 = world.spawn();

    ASSERT_TRUE(world.valid(e1));
    ASSERT_TRUE(world.valid(e2));
    ASSERT_NE(e1.id, e2.id);

    world.destroy(e1);
    scheduler.tickOnce(world, 0.016f);

    ASSERT_FALSE(world.valid(e1));
    ASSERT_TRUE(world.valid(e2));
}

TEST(Core, CommandBufferDestroyWithChildren) {
    Entelechy::World world;
    Entelechy::Scheduler scheduler;
    world.bindCommandBuffer(&scheduler.commandBuffer());

    auto parent = world.spawn();
    auto child = world.spawn();
    auto grandchild = world.spawn();

    world.setParent(child, parent);
    world.setParent(grandchild, child);

    scheduler.tickOnce(world, 0.016f);

    ASSERT_TRUE(world.hasComponent<Entelechy::ChildOf>(child));
    ASSERT_TRUE(world.hasComponent<Entelechy::ChildOf>(grandchild));
    ASSERT_TRUE(world.hasComponent<Entelechy::Children>(parent));
    ASSERT_TRUE(world.hasComponent<Entelechy::Children>(child));

    scheduler.commandBuffer().destroyWithChildren(parent);
    scheduler.tickOnce(world, 0.016f);

    ASSERT_FALSE(world.valid(parent));
    ASSERT_FALSE(world.valid(child));
    ASSERT_FALSE(world.valid(grandchild));
}

TEST(Core, BatchSpawnWithHierarchy) {
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

    ASSERT_TRUE(world.valid(entities[0]));
    ASSERT_TRUE(world.hasComponent<Entelechy::Position>(entities[0]));
    ASSERT_FALSE(world.hasComponent<Entelechy::Children>(root));

    scheduler.tickOnce(world, 0.016f);

    ASSERT_TRUE(world.hasComponent<Entelechy::Children>(root));
    const auto* children = world.getComponent<Entelechy::Children>(root);
    ASSERT_TRUE(children != nullptr);
    ASSERT_EQ(children->entities.size(), 50u);
}
