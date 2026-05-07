#pragma once
#include "vec.h"
#include <algorithm>

namespace Entelechy {

struct AABB {
    Vec3 min;
    Vec3 max;

    [[nodiscard]] static AABB fromCenterExtent(const Vec3& center, const Vec3& extent) {
        return {center - extent, center + extent};
    }

    [[nodiscard]] static AABB fromMinMax(const Vec3& minimum, const Vec3& maximum) {
        return {minimum, maximum};
    }

    void expand(const Vec3& point) {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
    }

    void expand(const AABB& other) {
        expand(other.min);
        expand(other.max);
    }

    [[nodiscard]] bool contains(const Vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    [[nodiscard]] bool intersects(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    [[nodiscard]] Vec3 center() const {
        return (min + max) * 0.5f;
    }

    [[nodiscard]] Vec3 size() const {
        return max - min;
    }

    [[nodiscard]] Vec3 extent() const {
        return size() * 0.5f;
    }

    [[nodiscard]] bool isEmpty() const {
        return min.x > max.x || min.y > max.y || min.z > max.z;
    }
};

} // namespace Entelechy
