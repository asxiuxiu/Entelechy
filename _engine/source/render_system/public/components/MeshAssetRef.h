#pragma once
#include "core/foundation_types.h"

namespace Entelechy
{

// MeshAssetRef — main-world component referencing a mesh asset.
// This is a lightweight asset identifier, NOT the engine's Handle<T>.
// The asset ID is resolved to GPU geometry during the Prepare phase.
struct MeshAssetRef
{
    u32 asset_id = 0xFFFFFFFFu;
};

} // namespace Entelechy
