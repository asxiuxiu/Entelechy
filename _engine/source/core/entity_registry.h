#pragma once
#include "foundation_types.h"
#include "dynamic_array.h"

namespace Entelechy {

struct Entity {
    u32 id{0xFFFFFFFFu};
    u32 generation{0};

    [[nodiscard]] bool valid() const { return id != 0xFFFFFFFFu; }
    constexpr auto operator<=>(const Entity& other) const = default;
    constexpr bool operator==(const Entity& other) const = default;
};

struct EntityRegistry {
    struct Slot {
        u32 generation = 0;
        bool alive = false;
    };

    EntityRegistry() = default;
    ~EntityRegistry() = default;

    EntityRegistry(const EntityRegistry&) = delete;
    EntityRegistry& operator=(const EntityRegistry&) = delete;

    [[nodiscard]] Entity create();
    [[nodiscard]] DynamicArray<Entity> allocateIDs(usize count);
    void destroy(Entity e);
    [[nodiscard]] bool isAlive(Entity e) const;
    [[nodiscard]] u32 getGeneration(u32 id) const;
    [[nodiscard]] usize aliveCount() const;
    [[nodiscard]] usize capacity() const;
    void clear();

private:
    DynamicArray<Slot> m_slots;
    DynamicArray<u32> m_free_list;
};

} // namespace Entelechy
