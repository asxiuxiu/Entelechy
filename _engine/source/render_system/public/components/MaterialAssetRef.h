#pragma once
#include "asset/handle/asset_handle.h"
#include "asset/type/material_asset.h"

namespace Entelechy
{

// MaterialAssetRef — main-world component referencing a material asset.
// The handle is resolved to a pipeline + parameter set during the Prepare phase.
struct MaterialAssetRef
{
    Handle<MaterialAsset> asset_id;
};

} // namespace Entelechy
