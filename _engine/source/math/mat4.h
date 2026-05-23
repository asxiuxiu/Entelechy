#pragma once
#include "vec.h"
#include "quat.h"
#include "math_config.h"
#include <cmath>

namespace Entelechy {

struct Mat4 {
    f32 m[16]; // column-major: m[col * 4 + row]

    [[nodiscard]] static Mat4 identity() {
        Mat4 out{};
        out.m[0] = out.m[5] = out.m[10] = out.m[15] = 1.0f;
        return out;
    }

    [[nodiscard]] static Mat4 zero() {
        return Mat4{};
    }

    f32 operator()(int row, int col) const { return m[col * 4 + row]; }
    f32& operator()(int row, int col) { return m[col * 4 + row]; }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                f32 sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += (*this)(i, k) * o(k, j);
                }
                r(i, j) = sum;
            }
        }
        return r;
    }

    [[nodiscard]] Mat4 transpose() const {
        Mat4 r{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                r(i, j) = (*this)(j, i);
            }
        }
        return r;
    }

    [[nodiscard]] f32 determinant() const {
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

    [[nodiscard]] Mat4 inverse() const {
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

    [[nodiscard]] Vec3 transformPoint(const Vec3& v) const {
        f32 x = m[0] * v.x + m[4] * v.y + m[8]  * v.z + m[12];
        f32 y = m[1] * v.x + m[5] * v.y + m[9]  * v.z + m[13];
        f32 z = m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14];
        f32 w = m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15];
        Vec3 result = w != 0.0f ? Vec3{x / w, y / w, z / w} : Vec3{x, y, z};
        MATH_CHECK_FINITE_3(result);
        return result;
    }

    [[nodiscard]] Vec3 transformVector(const Vec3& v) const {
        return {
            m[0] * v.x + m[4] * v.y + m[8]  * v.z,
            m[1] * v.x + m[5] * v.y + m[9]  * v.z,
            m[2] * v.x + m[6] * v.y + m[10] * v.z
        };
    }

    [[nodiscard]] static Mat4 fromTranslation(const Vec3& t) {
        Mat4 r = identity();
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    [[nodiscard]] static Mat4 fromScale(const Vec3& s) {
        Mat4 r = identity();
        r.m[0]  = s.x;
        r.m[5]  = s.y;
        r.m[10] = s.z;
        return r;
    }

    [[nodiscard]] static Mat4 fromRotation(const Quat& q);

    [[nodiscard]] static Mat4 fromTRS(const Vec3& t, const Quat& r, const Vec3& s) {
        return fromTranslation(t) * fromRotation(r) * fromScale(s);
    }

    [[nodiscard]] static Mat4 perspective(f32 fovY, f32 aspect, f32 nearZ, f32 farZ) {
        Mat4 r{};
        f32 tanHalfFov = std::tan(fovY * 0.5f);
        r.m[0] = 1.0f / (aspect * tanHalfFov);
        r.m[5] = 1.0f / tanHalfFov;
        r.m[10] = farZ / (nearZ - farZ);
        r.m[11] = -1.0f;
        r.m[14] = (nearZ * farZ) / (nearZ - farZ);
        return r;
    }

    [[nodiscard]] static Mat4 ortho(f32 left, f32 right, f32 bottom, f32 top, f32 nearZ, f32 farZ) {
        Mat4 r = identity();
        r.m[0]  = 2.0f / (right - left);
        r.m[5]  = 2.0f / (top - bottom);
        r.m[10] = 1.0f / (nearZ - farZ);
        r.m[12] = -(right + left) / (right - left);
        r.m[13] = -(top + bottom) / (top - bottom);
        r.m[14] = nearZ / (nearZ - farZ);
        return r;
    }

    [[nodiscard]] static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalized();
        Vec3 s = f.cross(up).normalized();
        Vec3 u = s.cross(f);
        Mat4 r = identity();
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -s.dot(eye);
        r.m[13] = -u.dot(eye);
        r.m[14] = f.dot(eye);
        return r;
    }
};

} // namespace Entelechy
