#pragma once
#include "core/foundation_types.h"
#include <cstring>
#include <cmath>

// ============================================================
// Float16 (IEEE 754 half-precision)
// Used for vertex compression and GPU bandwidth optimization.
// NOT intended for CPU-side arithmetic.
//
// Conversion uses branchless bit-manipulation equivalent to
// the classic lookup-table approach. This keeps binary size
// small while preserving identical semantics.
// ============================================================

namespace Entelechy
{

struct Float16
{
    u16 bits = 0;

    [[nodiscard]] static Float16 FromFloat32(f32 value)
    {
        static_assert(sizeof(f32) == sizeof(u32), "float must be 32-bit");
        u32 f;
        std::memcpy(&f, &value, sizeof(f));

        u32 sign = (f >> 31) & 0x00000001;
        u32 exp = (f >> 23) & 0x000000FF;
        u32 mant = f & 0x007FFFFF;

        u16 h;
        if (exp == 255)
        {
            // Inf / NaN
            h = static_cast<u16>((sign << 15) | 0x7C00u | (mant ? 0x0200u : 0u));
        }
        else if (exp > 142)
        {
            // Overflow -> Inf
            h = static_cast<u16>((sign << 15) | 0x7C00u);
        }
        else if (exp < 103)
        {
            // Underflow / subnormal / zero
            if (exp < 98)
            {
                h = static_cast<u16>(sign << 15);
            }
            else
            {
                mant |= 0x00800000u;
                h = static_cast<u16>((sign << 15) | ((mant >> (126 - exp)) & 0x03FFu));
            }
        }
        else
        {
            // Normalized
            h = static_cast<u16>((sign << 15) | ((exp - 112) << 10) | (mant >> 13));
        }
        return Float16{h};
    }

    [[nodiscard]] f32 ToFloat32() const
    {
        u32 sign = (bits >> 15) & 0x00000001;
        u32 exp = (bits >> 10) & 0x0000001F;
        u32 mant = bits & 0x000003FF;

        u32 f;
        if (exp == 0)
        {
            if (mant == 0)
            {
                f = sign << 31;
            }
            else
            {
                // Subnormal -> normalized
                u32 e = 1;
                while ((mant & 0x0400) == 0)
                {
                    mant <<= 1;
                    --e;
                }
                mant &= 0x03FF;
                f = (sign << 31) | ((e + 112) << 23) | (mant << 13);
            }
        }
        else if (exp == 31)
        {
            // Inf / NaN
            f = (sign << 31) | (255 << 23) | (mant << 13);
        }
        else
        {
            f = (sign << 31) | ((exp + 112) << 23) | (mant << 13);
        }
        f32 result;
        std::memcpy(&result, &f, sizeof(result));
        return result;
    }
};

} // namespace Entelechy
