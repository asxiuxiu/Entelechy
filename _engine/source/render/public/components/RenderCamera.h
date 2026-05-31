#pragma once
#include "core/foundation_types.h"
#include "core/math/mat4.h"
#include "core/math/frustum.h"
#include "ecs/type/type_registry.h"

namespace Entelechy {

// Viewport rectangle in pixel coordinates.
struct Rect {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 width = 0.0f;
    f32 height = 0.0f;
};

// ExtractedView — the camera snapshot living in the render world.
// Produced by ExtractCameraSystem each frame from (Camera + GlobalTransform).
struct ExtractedView {
    Mat4 view_matrix;
    Mat4 proj_matrix;
    Frustum frustum;
    Rect viewport;
};

REFLECT_COMPONENT(ExtractedView,
    REG_FIELD(ExtractedView, view_matrix, Mat4),
    REG_FIELD(ExtractedView, proj_matrix, Mat4),
    REG_FIELD(ExtractedView, frustum, Frustum),
    REG_FIELD(ExtractedView, viewport, Rect)
)

} // namespace Entelechy
