#pragma once
#include "core/foundation_types.h"

namespace Entelechy
{

enum class DefaultPhase : u8
{
    First = 0,
    PreUpdate = 1,
    Update = 2,
    PostUpdate = 3,
    Last = 4,
};

constexpr u8 DEFAULT_PHASE_COUNT = 5;

} // namespace Entelechy
