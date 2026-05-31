#pragma once

#include "core/math/math_config.h"
#include "core/math/align.h"
#include "core/math/vec.h"
#include "core/math/quat.h"
#include "core/math/mat4.h"
#include "core/math/aabb.h"
#include "core/math/random.h"
#include "core/math/simd.h"
#include "core/math/half.h"
#include "core/math/ray.h"
#include "core/math/frustum.h"

namespace Entelechy {

inline constexpr int MATH_LIB_VERSION = 2;

// Batch vector add exposed for testing / benchmarking
void BatchVec4Add(const f32* a, const f32* b, f32* out, usize count);

} // namespace Entelechy
