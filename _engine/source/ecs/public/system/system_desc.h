#pragma once
#include "core/foundation_types.h"
#include "core/container/dynamic_array.h"
#include "core/string/small_string.h"
#include "ecs/type/type_registry.h"
#include "ecs/world/phase.h"

namespace Entelechy {

class System;

struct SystemDesc {
    SmallString name;
    System* system = nullptr;
    u8 phase = static_cast<u8>(DefaultPhase::Update);
    DynamicArray<ComponentTypeID> reads;
    DynamicArray<ComponentTypeID> writes;
    DynamicArray<SmallString> before;
    DynamicArray<SmallString> after;
};

} // namespace Entelechy
