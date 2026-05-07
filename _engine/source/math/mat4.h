#pragma once
#include "vec.h"
#include "quat.h"
#include <cmath>

namespace Entelechy {

struct Mat4 {
    float m[16]; // column-major: m[col * 4 + row]

    [[nodiscard]] static Mat4 identity() {
        Mat4 out{};
        out.m[0] = out.m[5] = out.m[10] = out.m[15] = 1.0f;
        return out;
    }

    [[nodiscard]] static Mat4 zero() {
        return Mat4{};
    }

    float operator()(int row, int col) const { return m[col * 4 + row]; }
    float& operator()(int row, int col) { return m[col * 4 + row]; }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                float sum = 0.0f;
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

    [[nodiscard]] Vec3 transformPoint(const Vec3& v) const {
        float x = m[0] * v.x + m[4] * v.y + m[8]  * v.z + m[12];
        float y = m[1] * v.x + m[5] * v.y + m[9]  * v.z + m[13];
        float z = m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14];
        float w = m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15];
        return w != 0.0f ? Vec3{x / w, y / w, z / w} : Vec3{x, y, z};
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

    [[nodiscard]] static Mat4 perspective(float fovY, float aspect, float nearZ, float farZ) {
        Mat4 r{};
        float tanHalfFov = std::tan(fovY * 0.5f);
        r.m[0] = 1.0f / (aspect * tanHalfFov);
        r.m[5] = 1.0f / tanHalfFov;
        r.m[10] = farZ / (nearZ - farZ);
        r.m[11] = -1.0f;
        r.m[14] = (nearZ * farZ) / (nearZ - farZ);
        return r;
    }

    [[nodiscard]] static Mat4 ortho(float left, float right, float bottom, float top, float nearZ, float farZ) {
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
