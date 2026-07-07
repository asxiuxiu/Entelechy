#pragma once
#include "core/container/dynamic_array.h"
#include "core/foundation_types.h"

namespace Entelechy
{

struct FileData
{
    DynamicArray<u8> bytes;
    bool valid = false;
};

} // namespace Entelechy
