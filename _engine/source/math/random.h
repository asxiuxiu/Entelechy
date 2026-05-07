#pragma once
#include <cstdint>
#include <bit>
#include <cmath>

namespace Entelechy {

class RandomXORShift32 {
    uint32_t m_state;

public:
    explicit RandomXORShift32(uint32_t seed)
        : m_state(seed == 0 ? 0xBAD5EEDu : seed) {}

    [[nodiscard]] uint32_t nextUint32() {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return m_state;
    }

    [[nodiscard]] float nextFloat01() {
        constexpr uint32_t MAGIC = 0x3F800000u;
        return std::bit_cast<float>(MAGIC | (nextUint32() >> 9)) - 1.0f;
    }

    [[nodiscard]] float nextFloatRange(float minVal, float maxVal) {
        return minVal + nextFloat01() * (maxVal - minVal);
    }

    [[nodiscard]] uint32_t nextUintRange(uint32_t minVal, uint32_t maxVal) {
        return minVal + (nextUint32() % (maxVal - minVal + 1));
    }
};

} // namespace Entelechy
