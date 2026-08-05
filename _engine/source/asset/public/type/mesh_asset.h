#pragma once

namespace Entelechy
{

// ------------------------------------------------------------------
// MeshAsset — CPU-side mesh data (placeholder)
// ------------------------------------------------------------------
// Phase 2a only needs this type as the Handle<MeshAsset> template
// parameter. Vertex/index streams and the AABB are added in Phase 2b.
// Must remain default-constructible (HandleTable<T> requirement).
// ------------------------------------------------------------------
struct MeshAsset
{
};

} // namespace Entelechy
