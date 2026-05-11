#pragma once
#include "vec.h"
#include "mat4.h"
#include "aabb.h"
#include <cmath>

namespace Entelechy {

struct Frustum {
    // 6 planes in form (nx, ny, nz, d) where nx*x + ny*y + nz*z + d = 0
    Vec4 planes[6];

    enum PlaneIndex { Left = 0, Right, Bottom, Top, Near, Far };

    [[nodiscard]] static Frustum fromMatrix(const Mat4& m) {
        Frustum f;
        // Left
        f.planes[Left]   = Vec4{m.m[3]  + m.m[0],  m.m[7]  + m.m[4],  m.m[11] + m.m[8],   m.m[15] + m.m[12]};
        // Right
        f.planes[Right]  = Vec4{m.m[3]  - m.m[0],  m.m[7]  - m.m[4],  m.m[11] - m.m[8],   m.m[15] - m.m[12]};
        // Bottom
        f.planes[Bottom] = Vec4{m.m[3]  + m.m[1],  m.m[7]  + m.m[5],  m.m[11] + m.m[9],   m.m[15] + m.m[13]};
        // Top
        f.planes[Top]    = Vec4{m.m[3]  - m.m[1],  m.m[7]  - m.m[5],  m.m[11] - m.m[9],   m.m[15] - m.m[13]};
        // Near
        f.planes[Near]   = Vec4{m.m[3]  + m.m[2],  m.m[7]  + m.m[6],  m.m[11] + m.m[10],  m.m[15] + m.m[14]};
        // Far
        f.planes[Far]    = Vec4{m.m[3]  - m.m[2],  m.m[7]  - m.m[6],  m.m[11] - m.m[10],  m.m[15] - m.m[14]};

        // Normalize planes (only xyz part)
        for (auto& p : f.planes) {
            f32 len = std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
            if (len > 0.0f) {
                f32 invLen = 1.0f / len;
                p.x *= invLen; p.y *= invLen; p.z *= invLen; p.w *= invLen;
            }
        }
        return f;
    }

    [[nodiscard]] bool intersectsAABB(const AABB& box) const {
        for (const auto& p : planes) {
            Vec3 positiveVertex{
                p.x >= 0.0f ? box.max.x : box.min.x,
                p.y >= 0.0f ? box.max.y : box.min.y,
                p.z >= 0.0f ? box.max.z : box.min.z
            };
            f32 dist = p.x * positiveVertex.x + p.y * positiveVertex.y + p.z * positiveVertex.z + p.w;
            if (dist < 0.0f) return false;
        }
        return true;
    }
};

} // namespace Entelechy
