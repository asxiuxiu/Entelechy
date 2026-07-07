#pragma once
#include "core/foundation_types.h"
#include "core/math/math_config.h"

// ============================================================
// SIMD Abstraction Layer
// Compile-time polymorphism: no runtime CPU dispatch.
// All hot paths are inlineable.
// ============================================================

#if ARCH_X86
#include <immintrin.h>
#elif ARCH_ARM
#include <arm_neon.h>
#endif

namespace Entelechy::simd
{

#if ARCH_X86

using Vec4F = __m128;

inline Vec4F Load4F(const f32 *p)
{
    return _mm_loadu_ps(p);
}
inline void Store4F(f32 *p, Vec4F v)
{
    _mm_storeu_ps(p, v);
}
inline Vec4F Set1(f32 s)
{
    return _mm_set1_ps(s);
}
inline Vec4F Set(f32 x, f32 y, f32 z, f32 w)
{
    return _mm_set_ps(w, z, y, x);
}
inline Vec4F Zero()
{
    return _mm_setzero_ps();
}
inline Vec4F Add(Vec4F a, Vec4F b)
{
    return _mm_add_ps(a, b);
}
inline Vec4F Sub(Vec4F a, Vec4F b)
{
    return _mm_sub_ps(a, b);
}
inline Vec4F Mul(Vec4F a, Vec4F b)
{
    return _mm_mul_ps(a, b);
}
inline Vec4F Div(Vec4F a, Vec4F b)
{
    return _mm_div_ps(a, b);
}
inline Vec4F Neg(Vec4F v)
{
    return _mm_xor_ps(v, _mm_set1_ps(-0.0f));
}

// FMA: use _mm_fmadd_ps if AVX2 is available, else fallback to a*b + c
#if defined(__FMA__) || defined(__AVX2__)
inline Vec4F Fma(Vec4F a, Vec4F b, Vec4F c)
{
    return _mm_fmadd_ps(a, b, c);
}
#else
inline Vec4F Fma(Vec4F a, Vec4F b, Vec4F c)
{
    return _mm_add_ps(_mm_mul_ps(a, b), c);
}
#endif

inline f32 HorizontalSum(Vec4F v)
{
    __m128 shuf = _mm_movehdup_ps(v);  // [1,1,3,3]
    __m128 sums = _mm_add_ps(v, shuf); // [0+1, 1+1, 2+3, 3+3]
    shuf = _mm_movehl_ps(shuf, sums);  // [2+3, 3+3, 3, 3]
    sums = _mm_add_ss(sums, shuf);     // [0+1+2+3, ...]
    return _mm_cvtss_f32(sums);
}

#elif ARCH_ARM

using Vec4F = float32x4_t;

inline Vec4F Load4F(const f32 *p)
{
    return vld1q_f32(p);
}
inline void Store4F(f32 *p, Vec4F v)
{
    vst1q_f32(p, v);
}
inline Vec4F Set1(f32 s)
{
    return vdupq_n_f32(s);
}
inline Vec4F Set(f32 x, f32 y, f32 z, f32 w)
{
    alignas(16) f32 tmp[4] = {x, y, z, w};
    return vld1q_f32(tmp);
}
inline Vec4F Zero()
{
    return vdupq_n_f32(0.0f);
}
inline Vec4F Add(Vec4F a, Vec4F b)
{
    return vaddq_f32(a, b);
}
inline Vec4F Sub(Vec4F a, Vec4F b)
{
    return vsubq_f32(a, b);
}
inline Vec4F Mul(Vec4F a, Vec4F b)
{
    return vmulq_f32(a, b);
}
inline Vec4F Div(Vec4F a, Vec4F b)
{
    return vdivq_f32(a, b);
}
inline Vec4F Neg(Vec4F v)
{
    return vnegq_f32(v);
}
inline Vec4F Fma(Vec4F a, Vec4F b, Vec4F c)
{
    return vfmaq_f32(c, a, b);
}

inline f32 HorizontalSum(Vec4F v)
{
    float32x2_t sum2 = vadd_f32(vget_low_f32(v), vget_high_f32(v));
    return vget_lane_f32(vpadd_f32(sum2, sum2), 0);
}

#else
// Scalar fallback: keep 16-byte alignment for cross-arch layout consistency
struct alignas(16) Vec4F
{
    f32 v[4];
};

inline Vec4F Load4F(const f32 *p)
{
    return Vec4F{p[0], p[1], p[2], p[3]};
}
inline void Store4F(f32 *p, Vec4F v)
{
    p[0] = v.v[0];
    p[1] = v.v[1];
    p[2] = v.v[2];
    p[3] = v.v[3];
}
inline Vec4F Set1(f32 s)
{
    return Vec4F{s, s, s, s};
}
inline Vec4F Set(f32 x, f32 y, f32 z, f32 w)
{
    return Vec4F{x, y, z, w};
}
inline Vec4F Zero()
{
    return Vec4F{0.0f, 0.0f, 0.0f, 0.0f};
}
inline Vec4F Add(Vec4F a, Vec4F b)
{
    return Vec4F{a.v[0] + b.v[0], a.v[1] + b.v[1], a.v[2] + b.v[2], a.v[3] + b.v[3]};
}
inline Vec4F Sub(Vec4F a, Vec4F b)
{
    return Vec4F{a.v[0] - b.v[0], a.v[1] - b.v[1], a.v[2] - b.v[2], a.v[3] - b.v[3]};
}
inline Vec4F Mul(Vec4F a, Vec4F b)
{
    return Vec4F{a.v[0] * b.v[0], a.v[1] * b.v[1], a.v[2] * b.v[2], a.v[3] * b.v[3]};
}
inline Vec4F Div(Vec4F a, Vec4F b)
{
    return Vec4F{a.v[0] / b.v[0], a.v[1] / b.v[1], a.v[2] / b.v[2], a.v[3] / b.v[3]};
}
inline Vec4F Neg(Vec4F v)
{
    return Vec4F{-v.v[0], -v.v[1], -v.v[2], -v.v[3]};
}
inline Vec4F Fma(Vec4F a, Vec4F b, Vec4F c)
{
    return Vec4F{a.v[0] * b.v[0] + c.v[0], a.v[1] * b.v[1] + c.v[1], a.v[2] * b.v[2] + c.v[2],
                 a.v[3] * b.v[3] + c.v[3]};
}
inline f32 HorizontalSum(Vec4F v)
{
    return v.v[0] + v.v[1] + v.v[2] + v.v[3];
}

#endif

} // namespace Entelechy::simd
