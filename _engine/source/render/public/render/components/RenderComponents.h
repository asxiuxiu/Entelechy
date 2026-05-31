#pragma once
#include "core/foundation_types.h"
#include "core/math/mat4.h"
#include "ecs/type/type_registry.h"
#include "render/phase/RenderPhase.h"

namespace Entelechy {

// RenderMesh — render-world counterpart of MeshHandle.
// Contains only the asset ID; GPU geometry resolution happens later.
struct RenderMesh {
    u32 mesh_asset_id = 0xFFFFFFFFu;
};

// RenderMaterial — render-world counterpart of MaterialHandle.
struct RenderMaterial {
    u32 material_asset_id = 0xFFFFFFFFu;
    RenderPhase render_phase = RenderPhase::Opaque3D;
};

// RenderTransform — render-world counterpart of GlobalTransform.
// Copied verbatim during Extract so the render thread never touches main-world data.
struct RenderTransform {
    Mat4 world_matrix;
};

REFLECT_COMPONENT(RenderMesh,
    REG_FIELD(RenderMesh, mesh_asset_id, u32)
)

REFLECT_COMPONENT(RenderMaterial,
    REG_FIELD(RenderMaterial, material_asset_id, u32),
    REG_FIELD(RenderMaterial, render_phase, RenderPhase)
)

REFLECT_COMPONENT(RenderTransform,
    REG_FIELD(RenderTransform, world_matrix, Mat4)
)

} // namespace Entelechy
