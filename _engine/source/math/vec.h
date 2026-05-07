#pragma once
#include <cmath>
#include <cstdint>

namespace Entelechy {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator*(const Vec2& o) const { return {x * o.x, y * o.y}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }
    Vec2 operator-() const { return {-x, -y}; }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
    Vec2& operator/=(float s) { x /= s; y /= s; return *this; }

    [[nodiscard]] float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    [[nodiscard]] float length() const { return std::sqrt(dot(*this)); }
    [[nodiscard]] float lengthSq() const { return dot(*this); }

    [[nodiscard]] Vec2 normalized() const {
        float len = length();
        return len > 0.0f ? (*this) / len : Vec2{0.0f, 0.0f};
    }
};

inline Vec2 operator*(float s, const Vec2& v) { return v * s; }

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float& operator[](usize i) {
        CHECK(i < 3);
        return (&x)[i];
    }
    const float& operator[](usize i) const {
        CHECK(i < 3);
        return (&x)[i];
    }

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    Vec3& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }

    [[nodiscard]] float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] Vec3 cross(const Vec3& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
    [[nodiscard]] float length() const { return std::sqrt(dot(*this)); }
    [[nodiscard]] float lengthSq() const { return dot(*this); }

    [[nodiscard]] Vec3 normalized() const {
        float len = length();
        return len > 0.0f ? (*this) / len : Vec3{0.0f, 0.0f, 0.0f};
    }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    Vec4 operator+(const Vec4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Vec4 operator-(const Vec4& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    Vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    Vec4 operator*(const Vec4& o) const { return {x * o.x, y * o.y, z * o.z, w * o.w}; }
    Vec4 operator/(float s) const { return {x / s, y / s, z / s, w / s}; }
    Vec4 operator-() const { return {-x, -y, -z, -w}; }

    Vec4& operator+=(const Vec4& o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
    Vec4& operator-=(const Vec4& o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
    Vec4& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
    Vec4& operator/=(float s) { x /= s; y /= s; z /= s; w /= s; return *this; }

    [[nodiscard]] float dot(const Vec4& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    [[nodiscard]] float length() const { return std::sqrt(dot(*this)); }
    [[nodiscard]] float lengthSq() const { return dot(*this); }

    [[nodiscard]] Vec4 normalized() const {
        float len = length();
        return len > 0.0f ? (*this) / len : Vec4{0.0f, 0.0f, 0.0f, 0.0f};
    }
};

inline Vec4 operator*(float s, const Vec4& v) { return v * s; }

} // namespace Entelechy
