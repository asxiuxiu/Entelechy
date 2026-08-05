#pragma once
#include "asset/loader/asset_loader.h"
#include "asset/type/texture_asset.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// TextureAssetLoader — decodes image files into TextureAsset
// ------------------------------------------------------------------
// Uses stb_image; supports PNG/JPEG/BMP/TGA and the other stb_image
// formats. Output is always RGBA8. Called from the AssetServer IO
// thread, so the loader stays stateless.
// On decode failure the returned TextureAsset is empty (valid()
// == false); the error is logged with the source path.
// ------------------------------------------------------------------
class TextureAssetLoader : public IAssetLoader<TextureAsset>
{
public:
    TextureAsset load(const FileData &data, const Path &path) override;
};

} // namespace Entelechy
