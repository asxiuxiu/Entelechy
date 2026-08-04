#pragma once
#include "core/foundation_types.h"

namespace Entelechy
{

// MaterialAssetRef — main-world component referencing a material asset.
// This is a lightweight asset identifier, NOT the engine's Handle<T>.
// The asset ID is resolved to a pipeline + parameter set during the Prepare phase.
struct MaterialAssetRef
{
    u32 asset_id = 0xFFFFFFFFu;
};

} // namespace Entelechy
