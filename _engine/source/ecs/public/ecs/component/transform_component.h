#pragma once
#include "core/math/vec.h"
#include "core/math/quat.h"
#include "core/math/mat4.h"
#include "core/foundation_types.h"
#include "ecs/type/type_registry.h"

namespace Entelechy {

// Local transform component (editable by gameplay / AI / Inspector)
struct Transform {
    Vec3 translation;
    Quat rotation;
    Vec3 scale{1.0f, 1.0f, 1.0f};
    u32 dirty = 1;
};

REFLECT_COMPONENT(Transform,
    REG_FIELD(Transform, translation, Vec3),
    REG_FIELD(Transform, rotation, Quat),
    REG_FIELD(Transform, scale, Vec3)
)

// Cached world-space matrix (written by TransformPropagationSystem)
struct GlobalTransform {
    Mat4 matrix;
};

REFLECT_COMPONENT(GlobalTransform,
    REG_FIELD(GlobalTransform, matrix, Mat4)
)

// Dirty-flag marker for transform hierarchy changes.
// When present, the entity (and its subtree) needs GlobalTransform recomputed.
struct TransformTreeChanged {
    // Zero-sized marker; presence alone signals dirty state.
};

REFLECT_COMPONENT(TransformTreeChanged)

} // namespace Entelechy
