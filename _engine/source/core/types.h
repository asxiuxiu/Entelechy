#pragma once
#include "foundation_types.h"
#include <functional>
#include <compare>
#include "type_registry.h"
#include "small_string.h"
#include "string_id.h"

namespace Entelechy {

struct Entity {
    u32 id{0xFFFFFFFF};
    u32 generation{0};

    [[nodiscard]] bool valid() const { return id != 0xFFFFFFFF; }
    constexpr auto operator<=>(const Entity& other) const = default;
    constexpr bool operator==(const Entity& other) const = default;
};

struct Position {
    f32 x = 0.0f;
    f32 y = 0.0f;
};

REFLECT_COMPONENT(Position,
    REG_FIELD(Position, x, f32),
    REG_FIELD(Position, y, f32)
)

struct Velocity {
    f32 vx = 0.0f;
    f32 vy = 0.0f;
};

REFLECT_COMPONENT(Velocity,
    REG_FIELD(Velocity, vx, f32),
    REG_FIELD(Velocity, vy, f32)
)

struct Health {
    f32 hp = 100.0f;
};

REFLECT_COMPONENT(Health,
    REG_FIELD(Health, hp, f32)
)

struct NameTag {
    StringId name;
};

REFLECT_COMPONENT(NameTag,
    REG_FIELD(NameTag, name, StringId)
)

void registerBuiltinTypes();

} // namespace Entelechy

namespace std {
    template<>
    struct hash<Entelechy::Entity> {
        usize operator()(const Entelechy::Entity& e) const noexcept {
            return (static_cast<usize>(e.id) << 32) | static_cast<usize>(e.generation);
        }
    };
} // namespace std
