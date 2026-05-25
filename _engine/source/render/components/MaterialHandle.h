#pragma once
#include "foundation_types.h"
#include "type_registry.h"

namespace Entelechy {

// MaterialHandle — main-world component referencing a material asset.
// The asset ID is resolved to a pipeline + parameter set during the Prepare phase.
struct MaterialHandle {
    u32 asset_id = 0xFFFFFFFFu;
};

REFLECT_COMPONENT(MaterialHandle,
    REG_FIELD(MaterialHandle, asset_id, u32)
)

} // namespace Entelechy
