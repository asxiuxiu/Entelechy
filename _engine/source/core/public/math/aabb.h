#pragma once
#include "core/math/vec.h"
#include "core/math/mat4.h"
#include <algorithm>

namespace Entelechy
{

struct AABB
{
    Vec3 min;
    Vec3 max;

    [[nodiscard]] static AABB fromCenterExtent(const Vec3 &center, const Vec3 &extent)
    {
        return {center - extent, center + extent};
    }

    [[nodiscard]] static AABB fromMinMax(const Vec3 &minimum, const Vec3 &maximum)
    {
        return {minimum, maximum};
    }

    // World-space box of this local box under m: the 8 transformed
    // corners re-expanded into an axis-aligned box.
    [[nodiscard]] AABB transformed(const Mat4 &m) const
    {
        AABB out = fromMinMax(m.transformPoint(min), m.transformPoint(min));
        for (u32 i = 0; i < 8; ++i)
        {
            const Vec3 corner{(i & 1u) ? max.x : min.x, (i & 2u) ? max.y : min.y, (i & 4u) ? max.z : min.z};
            out.expand(m.transformPoint(corner));
        }
        return out;
    }

    void expand(const Vec3 &point)
    {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
    }

    [[nodiscard]] bool intersects(const AABB &other) const
    {
        return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    [[nodiscard]] bool contains(const Vec3 &point) const
    {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y && point.z >= min.z &&
               point.z <= max.z;
    }
};

} // namespace Entelechy
