#pragma once
#include "foundation_types.h"
#include "type_registry.h"

namespace Entelechy {

// Attached to every event entity so EventCleanupSystem knows when to destroy it.
struct EventLifetime {
    u64 frameCreated = 0;
};

REFLECT_COMPONENT(EventLifetime,
    REG_FIELD(EventLifetime, frameCreated, u64)
)

} // namespace Entelechy
