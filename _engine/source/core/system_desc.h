#pragma once
#include "foundation_types.h"
#include "dynamic_array.h"
#include "small_string.h"
#include "type_registry.h"

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
