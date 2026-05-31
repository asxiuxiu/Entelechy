#pragma once
#include "core/foundation_types.h"
#include <functional>
#include <compare>
#include "ecs/type/entity_registry.h"
#include "ecs/type/type_registry.h"
#include "core/string/small_string.h"
#include "core/string/string_id.h"

namespace Entelechy {

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

// ------------------------------------------------------------------
// Hierarchy relationship components (batch A foundation for batch B)
// ------------------------------------------------------------------
struct ChildOf {
    Entity parent;
};

REFLECT_COMPONENT(ChildOf,
    REG_FIELD(ChildOf, parent, Entity)
)

struct Children {
    DynamicArray<Entity> entities;
};

REFLECT_COMPONENT(Children)

struct Color {
    f32 r = 1.0f;
    f32 g = 1.0f;
    f32 b = 1.0f;
};

REFLECT_COMPONENT(Color,
    REG_FIELD(Color, r, f32),
    REG_FIELD(Color, g, f32),
    REG_FIELD(Color, b, f32)
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
