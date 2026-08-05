#pragma once
#include "core/foundation_types.h"
#include "core/math/aabb.h"
#include "core/math/mat4.h"
#include "asset/handle/asset_handle.h"
#include "asset/type/material_asset.h"
#include "asset/type/mesh_asset.h"
#include "render/phase/RenderPhase.h"

namespace Entelechy
{

// RenderMesh — render-world counterpart of MeshAssetRef.
// Contains only the asset handle; GPU geometry resolution happens later.
struct RenderMesh
{
    Handle<MeshAsset> mesh_asset_id;
};

// RenderMaterial — render-world counterpart of MaterialAssetRef.
struct RenderMaterial
{
    Handle<MaterialAsset> material_asset_id;
    RenderPhase render_phase = RenderPhase::Opaque3D;
};

// RenderTransform — render-world counterpart of GlobalTransform.
// Copied verbatim during Extract so the render thread never touches main-world data.
struct RenderTransform
{
    Mat4 world_matrix;
};

// RenderAABB — render-world counterpart of WorldAABB (world-space bounds).
// Copied during Extract; entities without it are always visible.
struct RenderAABB
{
    AABB box;
};

} // namespace Entelechy
