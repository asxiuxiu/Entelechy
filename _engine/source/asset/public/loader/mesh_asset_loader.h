#pragma once
#include "asset/loader/asset_loader.h"
#include "asset/type/mesh_asset.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// MeshAssetLoader — deserializes .emesh files into MeshAsset
// ------------------------------------------------------------------
// Reads the cooked binary format defined in asset/type/mesh_format.h
// (magic + version + counts + AABB + raw vertex/index arrays).
// Called from the AssetServer IO thread, so the loader stays
// stateless.
// On any validation failure (bad magic/version, truncated or
// length-mismatched data) the returned MeshAsset is empty and the
// error is logged with the source path. The stored AABB is trusted
// as-is: the cooker already ran computeBounds().
// ------------------------------------------------------------------
class MeshAssetLoader : public IAssetLoader<MeshAsset>
{
public:
    MeshAsset load(const FileData &data, const Path &path) override;
};

} // namespace Entelechy
