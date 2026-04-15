#pragma once
#include <cstddef>
#include <cstdint>

namespace Entelechy {

inline constexpr size_t AlignUp(size_t value, size_t alignment) {
    size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

template <typename T>
inline T* AlignUpPtr(T* ptr, size_t alignment) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t mask = static_cast<uintptr_t>(alignment - 1);
    uintptr_t aligned = (addr + mask) & ~mask;
    return reinterpret_cast<T*>(aligned);
}

inline constexpr size_t ALIGN_SIMD_16 = 16;
inline constexpr size_t ALIGN_GPU_BUFFER = 256;

} // namespace Entelechy
