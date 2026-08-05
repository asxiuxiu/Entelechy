#pragma once
#include "asset/handle/asset_handle.h"
#include "asset/type/texture_asset.h"
#include "core/math/vec.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// MaterialAsset — CPU-side material definition
// ------------------------------------------------------------------
// Phase 2c: unlit material — a base color multiplied with an optional
// base color texture. An invalid `base_color_texture` handle means
// untextured (the Prepare stage binds a 1x1 white fallback texture so
// the shader always samples).
// Named MaterialAsset to avoid clashing with the existing GPU-side
// Material class in render/material/material.h.
// Must remain default-constructible (HandleTable<T> requirement).
// ------------------------------------------------------------------
struct MaterialAsset
{
    Vec3 base_color{1.0f, 1.0f, 1.0f};
    Handle<TextureAsset> base_color_texture;
};

} // namespace Entelechy
