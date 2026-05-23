#pragma once
#include "foundation_types.h"
#include "entity_registry.h"
#include "type_registry.h"

namespace Entelechy {

// ------------------------------------------------------------------
// Built-in ECS event components
// Events are plain ECS entities with these components attached.
// They must be pure POD (no heap allocations) to keep spawn cost low.
// ------------------------------------------------------------------

struct KeyboardEvent {
    u32 keyCode = 0;
    bool pressed = false;
};

REFLECT_COMPONENT(KeyboardEvent,
    REG_FIELD(KeyboardEvent, keyCode, u32),
    REG_FIELD(KeyboardEvent, pressed, bool)
)

struct ColorChangeEvent {
    Entity target;
    f32 r = 1.0f;
    f32 g = 1.0f;
    f32 b = 1.0f;
};

REFLECT_COMPONENT(ColorChangeEvent,
    REG_FIELD(ColorChangeEvent, target, Entity),
    REG_FIELD(ColorChangeEvent, r, f32),
    REG_FIELD(ColorChangeEvent, g, f32),
    REG_FIELD(ColorChangeEvent, b, f32)
)

struct DeathEvent {
    Entity victim;
    Entity killer;
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
};

REFLECT_COMPONENT(DeathEvent,
    REG_FIELD(DeathEvent, victim, Entity),
    REG_FIELD(DeathEvent, killer, Entity),
    REG_FIELD(DeathEvent, x, f32),
    REG_FIELD(DeathEvent, y, f32),
    REG_FIELD(DeathEvent, z, f32)
)

} // namespace Entelechy
