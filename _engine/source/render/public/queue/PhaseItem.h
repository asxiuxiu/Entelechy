#pragma once
#include "core/foundation_types.h"
#include "ecs/type/entity_registry.h"
#include "render/phase/RenderPhase.h"
#include <algorithm>

namespace Entelechy
{

// 64-bit sort key: high 8 bits phase, mid 16 bits material, low 32 bits depth.
// Opaque: depth ascending (Early-Z). Transparent: depth descending.
union SortKey
{
    u64 value;
    struct
    {
        u32 depth;
        u16 material_id;
        u8 phase;
        u8 reserved;
    } packed;
};

// Encodes a view-space depth into a 32-bit unsigned key such that the integer
// order matches the original float order over [nearPlane, farPlane].
// Values outside the range are clamped: viewZ <= nearPlane maps to 0 and
// viewZ >= farPlane maps to 0xFFFFFFFF. This avoids the IEEE-754 bit-pattern
// ordering problem for negative or near-zero depths.
inline u32 encodeLinearDepth(f32 viewZ, f32 nearPlane, f32 farPlane)
{
    f32 range = farPlane - nearPlane;
    f32 normalized = 0.0f;
    if (range > 0.0f)
    {
        normalized = (viewZ - nearPlane) / range;
    }
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    if (normalized >= 1.0f)
    {
        return 0xFFFFFFFFu;
    }
    return static_cast<u32>(static_cast<f64>(normalized) * 4294967295.0);
}

struct PhaseItem
{
    SortKey sort_key;
    Entity render_entity;
    u32 instance_count = 1; // instancing reserved
};

} // namespace Entelechy
