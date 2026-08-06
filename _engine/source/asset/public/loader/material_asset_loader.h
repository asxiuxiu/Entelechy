#pragma once
#include "asset/loader/asset_loader.h"
#include "asset/type/material_asset.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// MaterialAssetLoader — deserializes .emat files into MaterialAsset
// ------------------------------------------------------------------
// Parses the fixed-schema JSON emitted by mesh_cooker:
//
//   {"base_color_texture":"sponza/textures/x.png",
//    "normal_texture":"...","mr_texture":"...",
//    "base_color_factor":[r,g,b],"metallic_factor":f,
//    "roughness_factor":f,"alpha_mode":"opaque|mask|blend",
//    "alpha_cutoff":f,"double_sided":false}
//
// Parsing uses the shared core JsonCursor (no JSON library). Missing
// keys keep the MaterialAsset defaults; any structural error (garbage,
// truncation, wrong value type, unknown key or alpha mode) rejects the
// file: the returned MaterialAsset is default-constructed and the
// error is logged with the source path.
// The loader only parses — texture fields land in the
// *_texture_path strings and it never triggers texture loads; the
// spawn side loadAsync's the paths and back-fills the Handle fields.
// Called from the AssetServer IO thread, so the loader stays
// stateless.
// ------------------------------------------------------------------
class MaterialAssetLoader : public IAssetLoader<MaterialAsset>
{
public:
    MaterialAsset load(const FileData &data, const Path &path) override;
};

} // namespace Entelechy
