#pragma once
#include "asset/handle/asset_handle.h"
#include "asset/type/mesh_asset.h"

namespace Entelechy
{

// MeshAssetRef — main-world component referencing a mesh asset.
// The handle is resolved to GPU geometry during the Prepare phase.
struct MeshAssetRef
{
    Handle<MeshAsset> asset_id;
};

} // namespace Entelechy
