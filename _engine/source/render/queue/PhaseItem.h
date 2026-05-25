#pragma once
#include "foundation_types.h"
#include "entity_registry.h"
#include "render/RenderPhase.h"

namespace Entelechy {

// 64-bit sort key: high 8 bits phase, mid 16 bits material, low 32 bits depth.
// Opaque: depth ascending (Early-Z). Transparent: depth descending.
union SortKey {
    u64 value;
    struct {
        u32 depth;
        u16 material_id;
        u8 phase;
        u8 reserved;
    } packed;
};

struct PhaseItem {
    SortKey sort_key;
    Entity render_entity;
    u32 instance_count = 1; // instancing reserved
};

} // namespace Entelechy
