#pragma once
#include "core/foundation_types.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace Entelechy {

class RandomXORShift32 {
    u32 m_state;

public:
    explicit RandomXORShift32(u32 seed)
        : m_state(seed == 0 ? 0xBAD5EEDu : seed) {}

    [[nodiscard]] u32 nextUint32() {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return m_state;
    }

    // Deterministic float in [0, 1) using bit-magic (IEEE 754)
    [[nodiscard]] f32 nextFloat01() {
        constexpr u32 MAGIC = 0x3F800000u; // 1.0f in IEEE 754
        u32 bits = MAGIC | (nextUint32() >> 9);
        f32 f;
        std::memcpy(&f, &bits, sizeof(f));
        return f - 1.0f;
    }

    [[nodiscard]] f32 nextFloatRange(f32 min, f32 max) {
        return min + nextFloat01() * (max - min);
    }
};

} // namespace Entelechy
