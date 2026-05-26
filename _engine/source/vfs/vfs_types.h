#pragma once
#include "dynamic_array.h"
#include "foundation_types.h"

namespace Entelechy {

struct FileData {
    DynamicArray<u8> bytes;
    bool valid = false;
};

} // namespace Entelechy
