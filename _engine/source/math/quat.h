#pragma once
#include "vec.h"
#include <cmath>

namespace Entelechy {

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    Quat operator*(const Quat& o) const {
        return {
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
            w * o.w - x * o.x - y * o.y - z * o.z
        };
    }

    Quat operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    Quat operator/(float s) const { return {x / s, y / s, z / s, w / s}; }
    Quat operator+(const Quat& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Quat operator-(const Quat& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    Quat operator-() const { return {-x, -y, -z, -w}; }

    [[nodiscard]] float dot(const Quat& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    [[nodiscard]] float lengthSq() const { return dot(*this); }
    [[nodiscard]] float length() const { return std::sqrt(lengthSq()); }

    [[nodiscard]] Quat normalized() const {
        float len = length();
        return len > 0.0f ? Quat{x / len, y / len, z / len, w / len} : Quat{0.0f, 0.0f, 0.0f, 1.0f};
    }

    [[nodiscard]] Quat conjugate() const { return {-x, -y, -z, w}; }
    [[nodiscard]] Quat inverse() const { return conjugate() / lengthSq(); }

    [[nodiscard]] static Quat identity() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    [[nodiscard]] static Quat fromAxisAngle(const Vec3& axis, float angle) {
        float half = angle * 0.5f;
        float s = std::sin(half);
        Vec3 n = axis.normalized();
        return {n.x * s, n.y * s, n.z * s, std::cos(half)};
    }

    [[nodiscard]] static Quat fromEuler(float pitch, float yaw, float roll) {
        float cy = std::cos(yaw * 0.5f);
        float sy = std::sin(yaw * 0.5f);
        float cp = std::cos(pitch * 0.5f);
        float sp = std::sin(pitch * 0.5f);
        float cr = std::cos(roll * 0.5f);
        float sr = std::sin(roll * 0.5f);
        return {
            sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy
        };
    }
};

[[nodiscard]] inline Vec3 rotate(const Quat& q, const Vec3& v) {
    Vec3 u = {q.x, q.y, q.z};
    float s = q.w;
    Vec3 cross1 = u.cross(v);
    Vec3 cross2 = u.cross(cross1);
    return v + cross1 * (2.0f * s) + cross2 * 2.0f;
}

[[nodiscard]] inline Quat slerp(const Quat& a, const Quat& b, float t) {
    float dot = a.dot(b);
    Quat b2 = b;
    if (dot < 0.0f) {
        dot = -dot;
        b2 = -b;
    }
    const float DOT_THRESHOLD = 0.9995f;
    if (dot > DOT_THRESHOLD) {
        Quat result = a + (b2 - a) * t;
        return result.normalized();
    }
    float theta0 = std::acos(dot);
    float theta = theta0 * t;
    float sinTheta = std::sin(theta);
    float sinTheta0 = std::sin(theta0);
    float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
    float s1 = sinTheta / sinTheta0;
    return a * s0 + b2 * s1;
}

} // namespace Entelechy
