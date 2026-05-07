#pragma once

#include "math_config.h"
#include "align.h"
#include "vec.h"
#include "quat.h"
#include "mat4.h"
#include "aabb.h"
#include "random.h"
#include "simd.h"
#include "half.h"
#include "ray.h"
#include "frustum.h"
#include "transform_component.h"

namespace Entelechy {

inline constexpr int MATH_LIB_VERSION = 2;

// Batch vector add exposed for testing / benchmarking
void BatchVec4Add(const float* a, const float* b, float* out, usize count);

} // namespace Entelechy
