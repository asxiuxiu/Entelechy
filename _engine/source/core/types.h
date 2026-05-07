#pragma once
#include "foundation_types.h"
#include <functional>
#include <compare>
#include "type_registry.h"
#include "small_string.h"

namespace Entelechy {

struct Entity {
    u32 id{0xFFFFFFFF};
    u32 generation{0};

    [[nodiscard]] bool valid() const { return id != 0xFFFFFFFF; }
    constexpr auto operator<=>(const Entity& other) const = default;
    constexpr bool operator==(const Entity& other) const = default;
};

struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

REFLECT_COMPONENT(Position,
    REG_FIELD(Position, x, float),
    REG_FIELD(Position, y, float)
)

struct Velocity {
    float vx = 0.0f;
    float vy = 0.0f;
};

REFLECT_COMPONENT(Velocity,
    REG_FIELD(Velocity, vx, float),
    REG_FIELD(Velocity, vy, float)
)

struct Health {
    float hp = 100.0f;
};

REFLECT_COMPONENT(Health,
    REG_FIELD(Health, hp, float)
)

struct NameTag {
    SmallString name;
};

REFLECT_COMPONENT(NameTag,
    REG_FIELD(NameTag, name, SmallString)
)

void registerBuiltinTypes();

} // namespace Entelechy

namespace std {
    template<>
    struct hash<Entelechy::Entity> {
        size_t operator()(const Entelechy::Entity& e) const noexcept {
            return (static_cast<size_t>(e.id) << 32) | static_cast<size_t>(e.generation);
        }
    };
} // namespace std
