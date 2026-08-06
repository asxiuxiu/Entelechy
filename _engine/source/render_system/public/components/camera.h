#pragma once
#include "core/foundation_types.h"

namespace Entelechy
{

// Camera — main-world component describing a view frustum.
// ExtractCameraSystem copies this + GlobalTransform into ExtractedView.
struct Camera
{
    f32 fov_y = 1.0472f; // vertical field of view in radians (~60°)
    f32 near_plane = 0.1f;
    f32 far_plane = 1000.0f;
    bool orthographic = false; // false = perspective, true = orthographic
    f32 ortho_size = 10.0f;    // half-height in world units when orthographic
};

} // namespace Entelechy
