#pragma once
#include "test_framework.h"
#include "math_lib.h"
#include <cmath>

namespace MathTest {

inline bool NearEq(f32 a, f32 b, f32 eps = 1e-5f) {
    return std::abs(a - b) < eps;
}

inline bool Vec2Near(const Entelechy::Vec2& a, const Entelechy::Vec2& b, f32 eps = 1e-5f) {
    return NearEq(a.x, b.x, eps) && NearEq(a.y, b.y, eps);
}

inline bool Vec3Near(const Entelechy::Vec3& a, const Entelechy::Vec3& b, f32 eps = 1e-4f) {
    return NearEq(a.x, b.x, eps) && NearEq(a.y, b.y, eps) && NearEq(a.z, b.z, eps);
}

inline bool Vec4Near(const Entelechy::Vec4& a, const Entelechy::Vec4& b, f32 eps = 1e-4f) {
    return NearEq(a.x, b.x, eps) && NearEq(a.y, b.y, eps) && NearEq(a.z, b.z, eps) && NearEq(a.w, b.w, eps);
}

inline bool Mat4Near(const Entelechy::Mat4& a, const Entelechy::Mat4& b, f32 eps = 1e-4f) {
    for (int i = 0; i < 16; ++i) {
        if (!NearEq(a.m[i], b.m[i], eps)) return false;
    }
    return true;
}

inline bool QuatNear(const Entelechy::Quat& a, const Entelechy::Quat& b, f32 eps = 1e-4f) {
    return NearEq(a.x, b.x, eps) && NearEq(a.y, b.y, eps) && NearEq(a.z, b.z, eps) && NearEq(a.w, b.w, eps);
}

} // namespace MathTest
