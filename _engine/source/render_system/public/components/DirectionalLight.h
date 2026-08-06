#pragma once
#include "core/foundation_types.h"
#include "core/math/vec.h"

namespace Entelechy
{

// DirectionalLight — main-world component describing a sun-like light.
// direction is the direction the light TRAVELS (from sun toward scene);
// it does not need to be normalized (ExtractLightSystem normalizes).
// ambient is a constant ambient term multiplied with albedo — a placeholder
// until IBL exists (see TODO.md).
// ExtractLightSystem copies the first one into ExtractedLight.
struct DirectionalLight
{
    Vec3 direction{0.0f, -1.0f, 0.0f};
    Vec3 color{1.0f, 1.0f, 1.0f};
    f32 intensity = 1.0f;
    f32 ambient = 0.03f;
};

} // namespace Entelechy
