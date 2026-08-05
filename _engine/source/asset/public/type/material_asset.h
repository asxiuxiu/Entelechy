#pragma once

namespace Entelechy
{

// ------------------------------------------------------------------
// MaterialAsset — CPU-side material definition (placeholder)
// ------------------------------------------------------------------
// Phase 2a only needs this type as the Handle<MaterialAsset> template
// parameter. Pipeline/parameter/texture fields are added in Phase 2b.
// Named MaterialAsset to avoid clashing with the existing GPU-side
// Material class in render/material/material.h.
// Must remain default-constructible (HandleTable<T> requirement).
// ------------------------------------------------------------------
struct MaterialAsset
{
};

} // namespace Entelechy
