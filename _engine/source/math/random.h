#pragma once
#include "foundation_types.h"
#include <bit>
#include <cmath>

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

    [[nodiscard]] f32 nextFloat01() {
        constexpr u32 MAGIC = 0x3F800000u;
        return std::bit_cast<f32>(MAGIC | (nextUint32() >> 9)) - 1.0f;
    }

    [[nodiscard]] f32 nextFloatRange(f32 minVal, f32 maxVal) {
        return minVal + nextFloat01() * (maxVal - minVal);
    }

    [[nodiscard]] u32 nextUintRange(u32 minVal, u32 maxVal) {
        return minVal + (nextUint32() % (maxVal - minVal + 1));
    }
};

} // namespace Entelechy
