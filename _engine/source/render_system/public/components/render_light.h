#pragma once
#include "core/foundation_types.h"
#include "core/math/vec.h"

namespace Entelechy
{

// ExtractedLight — the directional-light snapshot living in the render world.
// Produced by ExtractLightSystem each frame from the first main-world
// DirectionalLight (same single-instance pattern as ExtractedView).
struct ExtractedLight
{
    Vec3 direction{0.0f, -1.0f, 0.0f}; // normalized; direction the light travels
    Vec3 color{1.0f, 1.0f, 1.0f};
    f32 intensity = 1.0f;
    f32 ambient = 0.03f;
};

} // namespace Entelechy
