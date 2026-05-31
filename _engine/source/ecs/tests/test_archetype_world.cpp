#include "test/test_framework.h"
#include "ecs/archetype/archetype_world.h"
#include "ecs/type/types.h"

using namespace Entelechy;

TEST(ArchetypeWorld, SpawnAndDestroy) {
    ArchetypeWorld world;
    Entity e = world.spawn();
    ASSERT_TRUE(world.valid(e));
    ASSERT_EQ(world.entityCount(), 1u);
    world.destroy(e);
    ASSERT_FALSE(world.valid(e));
    ASSERT_EQ(world.entityCount(), 0u);
}

TEST(ArchetypeWorld, AddAndGetComponent) {
    ArchetypeWorld world;
    Entity e = world.spawn();
    Position* p = world.addComponent(e, Position{1.0f, 2.0f});
    ASSERT_TRUE(p != nullptr);
    ASSERT_EQ(p->x, 1.0f);
    ASSERT_EQ(p->y, 2.0f);

    Position* got = world.getComponent<Position>(e);
    ASSERT_TRUE(got != nullptr);
    ASSERT_EQ(got->x, 1.0f);
    ASSERT_EQ(got->y, 2.0f);
}

TEST(ArchetypeWorld, HasComponent) {
    ArchetypeWorld world;
    Entity e = world.spawn();
    ASSERT_FALSE(world.hasComponent<Position>(e));
    world.addComponent(e, Position{1.0f, 2.0f});
    ASSERT_TRUE(world.hasComponent<Position>(e));
}

TEST(ArchetypeWorld, RemoveComponent) {
    ArchetypeWorld world;
    Entity e = world.spawn();
    world.addComponent(e, Position{1.0f, 2.0f});
    ASSERT_TRUE(world.hasComponent<Position>(e));
    world.removeComponent<Position>(e);
    ASSERT_FALSE(world.hasComponent<Position>(e));
}

TEST(ArchetypeWorld, MultipleComponents) {
    ArchetypeWorld world;
    Entity e = world.spawn();
    world.addComponent(e, Position{1.0f, 2.0f});
    world.addComponent(e, Velocity{3.0f, 4.0f});

    Position* p = world.getComponent<Position>(e);
    Velocity* v = world.getComponent<Velocity>(e);
    ASSERT_TRUE(p != nullptr);
    ASSERT_TRUE(v != nullptr);
    ASSERT_EQ(p->x, 1.0f);
    ASSERT_EQ(v->vx, 3.0f);
}

TEST(ArchetypeWorld, QuerySingleComponent) {
    ArchetypeWorld world;
    Entity e1 = world.spawn();
    Entity e2 = world.spawn();
    world.addComponent(e1, Position{1.0f, 2.0f});
    world.addComponent(e2, Position{3.0f, 4.0f});

    usize count = 0;
    for (auto [e, p] : world.query<Position>()) {
        ASSERT_TRUE(p != nullptr);
        ++count;
    }
    ASSERT_EQ(count, 2u);
}

TEST(ArchetypeWorld, QueryMultipleComponents) {
    ArchetypeWorld world;
    Entity e1 = world.spawn();
    Entity e2 = world.spawn();
    world.addComponent(e1, Position{1.0f, 2.0f});
    world.addComponent(e1, Velocity{3.0f, 4.0f});
    world.addComponent(e2, Position{5.0f, 6.0f});

    usize count = 0;
    for (auto [e, p, v] : world.query<Position, Velocity>()) {
        ASSERT_TRUE(p != nullptr);
        ASSERT_TRUE(v != nullptr);
        ++count;
    }
    ASSERT_EQ(count, 1u);
}

TEST(ArchetypeWorld, DestroyEntityInQuery) {
    ArchetypeWorld world;
    Entity e1 = world.spawn();
    Entity e2 = world.spawn();
    world.addComponent(e1, Position{1.0f, 2.0f});
    world.addComponent(e2, Position{3.0f, 4.0f});

    world.destroy(e1);
    ASSERT_FALSE(world.valid(e1));
    ASSERT_TRUE(world.valid(e2));

    Position* p = world.getComponent<Position>(e2);
    ASSERT_TRUE(p != nullptr);
    ASSERT_EQ(p->x, 3.0f);
}

TEST(ArchetypeWorld, ComponentOverwrite) {
    ArchetypeWorld world;
    Entity e = world.spawn();
    world.addComponent(e, Position{1.0f, 2.0f});
    world.addComponent(e, Position{5.0f, 6.0f});
    Position* p = world.getComponent<Position>(e);
    ASSERT_EQ(p->x, 5.0f);
    ASSERT_EQ(p->y, 6.0f);
}

TEST(ArchetypeWorld, TypeErasedAPI) {
    ArchetypeWorld world;
    Entity e = world.spawn();
    Position pos{1.0f, 2.0f};
    world.addComponentRaw(e, TypeRegistry::instance().getTypeID<Position>(), &pos);
    ASSERT_TRUE(world.hasComponentRaw(e, TypeRegistry::instance().getTypeID<Position>()));
    const void* data = world.getComponentRaw(e, TypeRegistry::instance().getTypeID<Position>());
    ASSERT_TRUE(data != nullptr);
    ASSERT_EQ(static_cast<const Position*>(data)->x, 1.0f);
}
