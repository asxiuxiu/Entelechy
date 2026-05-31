#pragma once
#include "core/math/vec.h"
#include "core/math/math_config.h"
#include <cmath>

namespace Entelechy {

struct Quat {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 1.0f;

    Quat operator*(const Quat& o) const {
        return {
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
            w * o.w - x * o.x - y * o.y - z * o.z
        };
    }

    Quat operator*(f32 s) const { return {x * s, y * s, z * s, w * s}; }
    Quat operator/(f32 s) const { return {x / s, y / s, z / s, w / s}; }
    Quat operator+(const Quat& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Quat operator-(const Quat& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    Quat operator-() const { return {-x, -y, -z, -w}; }

    [[nodiscard]] f32 dot(const Quat& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    [[nodiscard]] f32 lengthSq() const { return dot(*this); }
    [[nodiscard]] f32 length() const { return std::sqrt(lengthSq()); }

    [[nodiscard]] Quat normalized() const {
        f32 len = length();
        MATH_CHECK_FINITE_QUAT((*this));
        return len > 0.0f ? Quat{x / len, y / len, z / len, w / len} : Quat{0.0f, 0.0f, 0.0f, 1.0f};
    }

    [[nodiscard]] Quat conjugate() const { return {-x, -y, -z, w}; }
    [[nodiscard]] Quat inverse() const { return conjugate() / lengthSq(); }

    [[nodiscard]] static Quat identity() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    [[nodiscard]] static Quat fromAxisAngle(const Vec3& axis, f32 angle) {
        f32 half = angle * 0.5f;
        f32 s = std::sin(half);
        Vec3 n = axis.normalized();
        return {n.x * s, n.y * s, n.z * s, std::cos(half)};
    }

    [[nodiscard]] static Quat fromEuler(f32 pitch, f32 yaw, f32 roll) {
        f32 cy = std::cos(yaw * 0.5f);
        f32 sy = std::sin(yaw * 0.5f);
        f32 cp = std::cos(pitch * 0.5f);
        f32 sp = std::sin(pitch * 0.5f);
        f32 cr = std::cos(roll * 0.5f);
        f32 sr = std::sin(roll * 0.5f);
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
    f32 s = q.w;
    Vec3 cross1 = u.cross(v);
    Vec3 cross2 = u.cross(cross1);
    return v + cross1 * (2.0f * s) + cross2 * 2.0f;
}

// Normalized Linear Interpolation: fast, good enough for 99% of cases
[[nodiscard]] inline Quat nlerp(const Quat& a, const Quat& b, f32 t) {
    f32 d = a.dot(b);
    f32 sign = d < 0.0f ? -1.0f : 1.0f;
    Quat r{
        a.x + (b.x * sign - a.x) * t,
        a.y + (b.y * sign - a.y) * t,
        a.z + (b.z * sign - a.z) * t,
        a.w + (b.w * sign - a.w) * t
    };
    return r.normalized();
}

// Spherical Linear Interpolation: constant angular velocity, more expensive
[[nodiscard]] inline Quat slerp(const Quat& a, const Quat& b, f32 t) {
    f32 dot = a.dot(b);
    Quat b2 = b;
    if (dot < 0.0f) {
        dot = -dot;
        b2 = -b;
    }
    const f32 DOT_THRESHOLD = 0.9995f;
    if (dot > DOT_THRESHOLD) {
        Quat result = a + (b2 - a) * t;
        return result.normalized();
    }
    f32 theta0 = std::acos(dot);
    f32 theta = theta0 * t;
    f32 sinTheta = std::sin(theta);
    f32 sinTheta0 = std::sin(theta0);
    f32 s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
    f32 s1 = sinTheta / sinTheta0;
    return a * s0 + b2 * s1;
}

} // namespace Entelechy
