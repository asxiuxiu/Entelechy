#pragma once
#include "core/foundation_types.h"
#include "ecs/type/type_registry.h"

namespace Entelechy
{

// MeshAssetRef — main-world component referencing a mesh asset.
// This is a lightweight asset identifier, NOT the engine's Handle<T>.
// The asset ID is resolved to GPU geometry during the Prepare phase.
struct MeshAssetRef
{
    u32 asset_id = 0xFFFFFFFFu;
};

REFLECT_COMPONENT(MeshAssetRef, REG_FIELD(MeshAssetRef, asset_id, u32))

} // namespace Entelechy
