#pragma once
#include <cstdint>
#include <string>
#include "type_registry.h"

using Entity = uint32_t;

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
    std::string name;
};

REFLECT_COMPONENT(NameTag,
    REG_FIELD(NameTag, name, std::string)
)

namespace Entelechy {

void registerBuiltinTypes();

} // namespace Entelechy
