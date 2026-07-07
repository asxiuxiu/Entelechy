#pragma once
#include "core/foundation_types.h"

namespace Entelechy
{

inline constexpr usize AlignUp(usize value, usize alignment)
{
    usize mask = alignment - 1;
    return (value + mask) & ~mask;
}

template <typename T>
inline T *AlignUpPtr(T *ptr, usize alignment)
{
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t mask = static_cast<uintptr_t>(alignment - 1);
    uintptr_t aligned = (addr + mask) & ~mask;
    return reinterpret_cast<T *>(aligned);
}

inline constexpr usize ALIGN_SIMD_16 = 16;
inline constexpr usize ALIGN_GPU_BUFFER = 256;

} // namespace Entelechy
