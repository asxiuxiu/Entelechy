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

f32 Mat4::determinant() const {
    f32 s0 = m[0]*m[5] - m[1]*m[4];
    f32 s1 = m[0]*m[6] - m[2]*m[4];
    f32 s2 = m[0]*m[7] - m[3]*m[4];
    f32 s3 = m[1]*m[6] - m[2]*m[5];
    f32 s4 = m[1]*m[7] - m[3]*m[5];
    f32 s5 = m[2]*m[7] - m[3]*m[6];
    f32 c5 = m[10]*m[15] - m[11]*m[14];
    f32 c4 = m[9]*m[15]  - m[11]*m[13];
    f32 c3 = m[9]*m[14]  - m[10]*m[13];
    f32 c2 = m[8]*m[15]  - m[11]*m[12];
    f32 c1 = m[8]*m[14]  - m[10]*m[12];
    f32 c0 = m[8]*m[13]  - m[9]*m[12];
    return s0*c5 - s1*c4 + s2*c3 + s3*c2 - s4*c1 + s5*c0;
}

Mat4 Mat4::inverse() const {
    f32 inv[16];
    inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
    inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
    inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];
    inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7]  + m[9]*m[3]*m[6];
    inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7]  - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]  + m[4]*m[1]*m[11] - m[4]*m[3]*m[9]  - m[8]*m[1]*m[7]  + m[8]*m[3]*m[5];
    inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]  - m[4]*m[1]*m[10] + m[4]*m[2]*m[9]  + m[8]*m[1]*m[6]  - m[8]*m[2]*m[5];

    f32 det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (std::abs(det) < 1e-6f) return Mat4::zero();

    f32 invDet = 1.0f / det;
    Mat4 out{};
    for (int i = 0; i < 16; ++i) out.m[i] = inv[i] * invDet;
    return out;
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
