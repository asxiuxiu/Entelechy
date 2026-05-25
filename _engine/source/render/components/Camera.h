#pragma once
#include "foundation_types.h"
#include "type_registry.h"

namespace Entelechy {

// Camera — main-world component describing a view frustum.
// ExtractCameraSystem copies this + GlobalTransform into ExtractedView.
struct Camera {
    f32 fov_y = 1.0472f;        // vertical field of view in radians (~60°)
    f32 near_plane = 0.1f;
    f32 far_plane = 1000.0f;
    bool orthographic = false;  // false = perspective, true = orthographic
    f32 ortho_size = 10.0f;     // half-height in world units when orthographic
};

REFLECT_COMPONENT(Camera,
    REG_FIELD(Camera, fov_y, f32),
    REG_FIELD(Camera, near_plane, f32),
    REG_FIELD(Camera, far_plane, f32),
    REG_FIELD(Camera, orthographic, bool),
    REG_FIELD(Camera, ortho_size, f32)
)

} // namespace Entelechy
