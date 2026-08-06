#pragma once
#include "core/foundation_types.h"
#include "core/math/mat4.h"
#include "core/math/vec.h"
#include "core/math/frustum.h"

namespace Entelechy
{

// Viewport rectangle in pixel coordinates.
struct Rect
{
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 width = 0.0f;
    f32 height = 0.0f;
};

// ExtractedView — the camera snapshot living in the render world.
// Produced by ExtractCameraSystem each frame from (Camera + GlobalTransform).
struct ExtractedView
{
    Mat4 view_matrix;
    Mat4 proj_matrix;
    Frustum frustum;
    Rect viewport;
    f32 near_plane = 0.1f;
    f32 far_plane = 1000.0f;
    Vec3 view_pos; // camera world position (for view-dependent lighting)
};

} // namespace Entelechy
