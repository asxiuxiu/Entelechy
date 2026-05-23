#include "math_lib.h"
#include "simd.h"
#include <cmath>

namespace Entelechy {

Mat4 Mat4::fromRotation(const Quat& q) {
    f32 x2 = q.x * 2.0f;
    f32 y2 = q.y * 2.0f;
    f32 z2 = q.z * 2.0f;
    f32 wx = q.w * x2;
    f32 wy = q.w * y2;
    f32 wz = q.w * z2;
    f32 xx = q.x * x2;
    f32 xy = q.x * y2;
    f32 xz = q.x * z2;
    f32 yy = q.y * y2;
    f32 yz = q.y * z2;
    f32 zz = q.z * z2;

    Mat4 r{};
    r.m[0]  = 1.0f - (yy + zz);
    r.m[1]  = xy + wz;
    r.m[2]  = xz - wy;
    r.m[3]  = 0.0f;
    r.m[4]  = xy - wz;
    r.m[5]  = 1.0f - (xx + zz);
    r.m[6]  = yz + wx;
    r.m[7]  = 0.0f;
    r.m[8]  = xz + wy;
    r.m[9]  = yz - wx;
    r.m[10] = 1.0f - (xx + yy);
    r.m[11] = 0.0f;
    r.m[12] = 0.0f;
    r.m[13] = 0.0f;
    r.m[14] = 0.0f;
    r.m[15] = 1.0f;
    return r;
}

// Minimal batch add to validate simd abstraction layer
void BatchVec4Add(const f32* a, const f32* b, f32* out, usize count) {
    usize i = 0;
#if ARCH_X86 || ARCH_ARM
    for (; i + 4 <= count; i += 4) {
        simd::Vec4F va = simd::Load4F(a + i);
        simd::Vec4F vb = simd::Load4F(b + i);
        simd::Store4F(out + i, simd::Add(va, vb));
    }
#endif
    for (; i < count; ++i) {
        out[i] = a[i] + b[i];
    }
}

} // namespace Entelechy
