#pragma once
#include "asset/handle/asset_handle.h"
#include "asset/type/texture_asset.h"
#include "core/math/vec.h"
#include "core/string/string.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// AlphaMode — glTF alphaMode semantics
// ------------------------------------------------------------------
// Opaque: ignore alpha. Mask: discard fragments below alpha_cutoff.
// Blend: treated as opaque for now (correct blending needs sorting; see
// TODO.md).
// ------------------------------------------------------------------
enum class AlphaMode : u8
{
    Opaque,
    Mask,
    Blend,
};

// ------------------------------------------------------------------
// MaterialAsset — CPU-side material definition
// ------------------------------------------------------------------
// Unlit material: a base color multiplied with an optional base color
// texture. An invalid `base_color_texture` handle means untextured (the
// Prepare stage binds a 1x1 white fallback texture so the shader always
// samples). The former white-model `shade_mode` scalar switch was retired
// once textured materials always took the albedo path.
// glTF pbrMetallicRoughness fields cooked into .emat. `base_color`
// doubles as baseColorFactor — glTF's factor is RGBA but the engine keeps
// Vec3 and drops A (opacity comes from the texture; a constant-factor
// alpha has no consumer until the lighting phase). normal/MR textures are
// parsed and loaded and sampled by the lit PBR shader (normal map perturbs
// N via TBN; MR texture multiplies the factors, G=roughness/B=metallic).
// The `*_texture_path` strings hold the content-relative texture paths
// parsed from .emat; the scene spawn side loadAsync's them and
// back-fills the Handle fields. The loader itself never triggers texture
// loads. Path/Handle dual storage is transitional — see TODO.md.
// Named MaterialAsset to avoid clashing with the existing GPU-side
// Material class in render/material/material.h.
// Must remain default-constructible (HandleTable<T> requirement).
// ------------------------------------------------------------------
struct MaterialAsset
{
    Vec3 base_color{1.0f, 1.0f, 1.0f};
    Handle<TextureAsset> base_color_texture;

    f32 metallic_factor = 1.0f;
    f32 roughness_factor = 1.0f;
    Handle<TextureAsset> normal_texture;
    Handle<TextureAsset> mr_texture;
    AlphaMode alpha_mode = AlphaMode::Opaque;
    f32 alpha_cutoff = 0.5f;
    bool double_sided = false;

    String base_color_texture_path;
    String normal_texture_path;
    String mr_texture_path;
};

} // namespace Entelechy
