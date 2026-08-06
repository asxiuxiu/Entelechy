#pragma once
#include "core/foundation_types.h"
#include "core/math/vec.h"

namespace Entelechy
{

// ExtractedSky — the sky-settings snapshot living in the render world.
// Produced by ExtractSkySystem each frame from the first main-world
// SkySettings (same single-instance pattern as ExtractedView/ExtractedLight).
struct ExtractedSky
{
    Vec3 zenith_color{0.10f, 0.23f, 0.55f};
    Vec3 horizon_color{0.55f, 0.65f, 0.75f};
    bool enabled = true;
};

} // namespace Entelechy
