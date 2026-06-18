#pragma once
#include "core/foundation_types.h"
#include <cstring>
#include <type_traits>
#include <utility>

namespace Entelechy {

// Stable LSD radix sort for up to 64-bit unsigned keys.
//
// Sorts [data, data + count) using the key returned by keyFunc(item).
// `scratch` must point to a buffer with at least `count` elements.
// After the call, `data` holds the sorted range in ascending key order.
//
// Requirements:
//   - T is copy-assignable (the algorithm copies values into scratch).
//   - keyFunc(const T&) returns a u64 (the key is treated as unsigned).
template<typename T, typename KeyFunc>
void radixSort64(T* data, usize count, KeyFunc&& keyFunc, T* scratch) {
    STATIC_ASSERT(std::is_copy_assignable_v<T>, "radixSort64 requires copy-assignable values");

    if (count <= 1) {
        return;
    }

    T* src = data;
    T* dst = scratch;

    usize histogram[256];
    usize offset[256];

    for (int pass = 0; pass < 8; ++pass) {
        const int shift = pass * 8;

        std::memset(histogram, 0, sizeof(histogram));
        for (usize i = 0; i < count; ++i) {
            const u64 key = keyFunc(src[i]);
            ++histogram[(key >> shift) & 0xFFULL];
        }

        usize sum = 0;
        for (int i = 0; i < 256; ++i) {
            offset[i] = sum;
            sum += histogram[i];
        }

        for (usize i = 0; i < count; ++i) {
            const u64 key = keyFunc(src[i]);
            const u8 byte = static_cast<u8>((key >> shift) & 0xFFULL);
            dst[offset[byte]++] = src[i];
        }

        std::swap(src, dst);
    }

    // If the last pass wrote to scratch, copy the result back into data.
    if (src != data) {
        for (usize i = 0; i < count; ++i) {
            data[i] = src[i];
        }
    }
}

} // namespace Entelechy
