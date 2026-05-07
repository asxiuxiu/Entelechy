#pragma once
#include "vec.h"
#include "quat.h"
#include "mat4.h"
#include "foundation_types.h"

namespace Entelechy {

// Local transform component (editable by gameplay / AI / Inspector)
struct Transform {
    Vec3 translation;
    Quat rotation;
    Vec3 scale{1.0f, 1.0f, 1.0f};
    u32 dirty : 1;

    Transform() : dirty(1) {}
};

// Cached world-space matrix (written by TransformPropagationSystem)
struct GlobalTransform {
    Mat4 matrix;
};

} // namespace Entelechy
