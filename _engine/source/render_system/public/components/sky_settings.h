#pragma once
#include "core/foundation_types.h"
#include "core/math/vec.h"

namespace Entelechy
{

// SkySettings — main-world component describing the gradient sky pass.
// The sky is a fullscreen triangle drawn right after clear
// and before the opaque phase, colored by interpolating between
// horizon_color and zenith_color along the view ray's y component. It is NOT
// a skybox — the asset pack contains none (roadmap defers it, see TODO.md).
// Colors are authored in the same approximate-gamma space as the lit shader
// (the sky fragment shader applies the same pow(1/2.2) output transform).
// ExtractSkySystem copies the first one into ExtractedSky.
struct SkySettings
{
    Vec3 zenith_color{0.10f, 0.23f, 0.55f};
    Vec3 horizon_color{0.55f, 0.65f, 0.75f};
    bool enabled = true;
};

} // namespace Entelechy
