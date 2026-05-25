#pragma once
#include "foundation_types.h"
#include "type_registry.h"

namespace Entelechy {

// MeshHandle — main-world component referencing a mesh asset.
// The asset ID is resolved to GPU geometry during the Prepare phase.
struct MeshHandle {
    u32 asset_id = 0xFFFFFFFFu;
};

REFLECT_COMPONENT(MeshHandle,
    REG_FIELD(MeshHandle, asset_id, u32)
)

} // namespace Entelechy
