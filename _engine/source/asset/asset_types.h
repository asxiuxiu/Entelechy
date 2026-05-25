#pragma once
#include "foundation_types.h"

namespace Entelechy {

// ------------------------------------------------------------------
// AssetLoadState — precise lifecycle of an asset
// ------------------------------------------------------------------
enum class AssetLoadState : u8 {
    Unloaded,  // not started
    Pending,   // load request submitted, waiting for IO thread
    Loading,   // IO thread is reading/decoding
    Loaded,    // data in memory, but dependencies not yet resolved
    Ready,     // data + dependencies resolved, safe to use
    Failed,    // load failed (file missing, format error, etc.)
    Unloading, // reference count reached zero, transitioning out
};

using AssetTypeID = u32;
constexpr AssetTypeID INVALID_ASSET_TYPE_ID = 0xFFFFFFFFu;

} // namespace Entelechy
