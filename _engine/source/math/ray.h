#pragma once
#include "vec.h"
#include "aabb.h"
#include <cmath>
#include <cfloat>
#include <algorithm>

namespace Entelechy {

struct Ray {
    Vec3 origin;
    Vec3 direction; // must be normalized

    [[nodiscard]] bool intersectAABB(const AABB& box, f32& out_t) const {
        f32 tmin = 0.0f, tmax = FLT_MAX;
        for (int i = 0; i < 3; ++i) {
            if (std::abs(direction[i]) < 1e-6f) {
                if (origin[i] < box.min[i] || origin[i] > box.max[i]) return false;
            } else {
                f32 ood = 1.0f / direction[i];
                f32 t1 = (box.min[i] - origin[i]) * ood;
                f32 t2 = (box.max[i] - origin[i]) * ood;
                tmin = std::max(tmin, std::min(t1, t2));
                tmax = std::min(tmax, std::max(t1, t2));
                if (tmin > tmax) return false;
            }
        }
        out_t = tmin;
        return true;
    }
};

} // namespace Entelechy
