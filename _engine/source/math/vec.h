#pragma once
#include "foundation_types.h"
#include "math_config.h"
#include <cmath>
#include <cstdint>

namespace Entelechy {

struct Vec2 {
    f32 x = 0.0f;
    f32 y = 0.0f;

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(f32 s) const { return {x * s, y * s}; }
    Vec2 operator*(const Vec2& o) const { return {x * o.x, y * o.y}; }
    Vec2 operator/(f32 s) const { return {x / s, y / s}; }
    Vec2 operator-() const { return {-x, -y}; }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(f32 s) { x *= s; y *= s; return *this; }
    Vec2& operator/=(f32 s) { x /= s; y /= s; return *this; }

    [[nodiscard]] f32 dot(const Vec2& o) const { return x * o.x + y * o.y; }
    [[nodiscard]] f32 length() const { return std::sqrt(dot(*this)); }
    [[nodiscard]] f32 lengthSq() const { return dot(*this); }

    [[nodiscard]] Vec2 normalized() const {
        f32 len = length();
        MATH_CHECK_FINITE_2((*this));
        return len > 0.0f ? (*this) / len : Vec2{0.0f, 0.0f};
    }
};

inline Vec2 operator*(f32 s, const Vec2& v) { return v * s; }

struct Vec3 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;

    f32& operator[](usize i) {
        CHECK(i < 3);
        return (&x)[i];
    }
    const f32& operator[](usize i) const {
        CHECK(i < 3);
        return (&x)[i];
    }

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(f32 s) const { return {x * s, y * s, z * s}; }
    Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    Vec3 operator/(f32 s) const { return {x / s, y / s, z / s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(f32 s) { x *= s; y *= s; z *= s; return *this; }
    Vec3& operator/=(f32 s) { x /= s; y /= s; z /= s; return *this; }

    [[nodiscard]] f32 dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] Vec3 cross(const Vec3& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
    [[nodiscard]] f32 length() const { return std::sqrt(dot(*this)); }
    [[nodiscard]] f32 lengthSq() const { return dot(*this); }

    [[nodiscard]] Vec3 normalized() const {
        f32 len = length();
        MATH_CHECK_FINITE_3((*this));
        return len > 0.0f ? (*this) / len : Vec3{0.0f, 0.0f, 0.0f};
    }
};

inline Vec3 operator*(f32 s, const Vec3& v) { return v * s; }

// SIMD computation format: 16-byte aligned, 4 floats
struct alignas(16) Vec4 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;

    Vec4 operator+(const Vec4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Vec4 operator-(const Vec4& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    Vec4 operator*(f32 s) const { return {x * s, y * s, z * s, w * s}; }
    Vec4 operator*(const Vec4& o) const { return {x * o.x, y * o.y, z * o.z, w * o.w}; }
    Vec4 operator/(f32 s) const { return {x / s, y / s, z / s, w / s}; }
    Vec4 operator-() const { return {-x, -y, -z, -w}; }

    Vec4& operator+=(const Vec4& o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
    Vec4& operator-=(const Vec4& o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
    Vec4& operator*=(f32 s) { x *= s; y *= s; z *= s; w *= s; return *this; }
    Vec4& operator/=(f32 s) { x /= s; y /= s; z /= s; w /= s; return *this; }

    [[nodiscard]] f32 dot(const Vec4& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    [[nodiscard]] f32 length() const { return std::sqrt(dot(*this)); }
    [[nodiscard]] f32 lengthSq() const { return dot(*this); }

    [[nodiscard]] Vec4 normalized() const {
        f32 len = length();
        MATH_CHECK_FINITE_4((*this));
        return len > 0.0f ? (*this) / len : Vec4{0.0f, 0.0f, 0.0f, 0.0f};
    }
};

inline Vec4 operator*(f32 s, const Vec4& v) { return v * s; }

} // namespace Entelechy
