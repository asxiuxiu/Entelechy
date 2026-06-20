#pragma once
#include "core/foundation_types.h"
#include "ecs/type/type_registry.h"

namespace Entelechy {

// MaterialAssetRef — main-world component referencing a material asset.
// This is a lightweight asset identifier, NOT the engine's Handle<T>.
// The asset ID is resolved to a pipeline + parameter set during the Prepare phase.
struct MaterialAssetRef {
    u32 asset_id = 0xFFFFFFFFu;
};

REFLECT_COMPONENT(MaterialAssetRef,
    REG_FIELD(MaterialAssetRef, asset_id, u32)
)

} // namespace Entelechy
