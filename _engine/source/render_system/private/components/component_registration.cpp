// component_registration.cpp — centralized ECS component registration for all
// render_system components. REFLECT_COMPONENT calls were relocated here to keep
// component headers focused on data definitions.
//
// Every REFLECT_COMPONENT lives in exactly one .obj inside RenderSystemLib.
// registerRenderComponents() is a no-op function whose sole purpose is to force
// MSVC's linker to pull this translation unit from RenderSystemLib.lib; the
// anonymous-namespace static initializers run before main() as a side effect.

#include "render_system/components/Camera.h"
#include "render_system/components/MaterialAssetRef.h"
#include "render_system/components/MeshAssetRef.h"
#include "render_system/components/RenderCamera.h"
#include "render_system/components/RenderComponents.h"
#include "render_system/culling/ViewVisibleList.h"
#include "render_system/phase/RenderResources.h"
#include "ecs/type/type_registry.h"

namespace Entelechy
{

// -- main-world components (used by extract systems) --

REFLECT_COMPONENT(Camera, REG_FIELD(Camera, fov_y, f32), REG_FIELD(Camera, near_plane, f32),
                  REG_FIELD(Camera, far_plane, f32), REG_FIELD(Camera, orthographic, bool),
                  REG_FIELD(Camera, ortho_size, f32))

REFLECT_COMPONENT(MeshAssetRef, REG_FIELD(MeshAssetRef, asset_id, u32))

REFLECT_COMPONENT(MaterialAssetRef, REG_FIELD(MaterialAssetRef, asset_id, u32))

// -- render-world components (spawned/extracted by systems) --

REFLECT_COMPONENT(RenderMesh, REG_FIELD(RenderMesh, mesh_asset_id, u32))

REFLECT_COMPONENT(RenderMaterial, REG_FIELD(RenderMaterial, material_asset_id, u32),
                  REG_FIELD(RenderMaterial, render_phase, RenderPhase))

REFLECT_COMPONENT(RenderTransform, REG_FIELD(RenderTransform, world_matrix, Mat4))

REFLECT_COMPONENT(ExtractedView, REG_FIELD(ExtractedView, view_matrix, Mat4),
                  REG_FIELD(ExtractedView, proj_matrix, Mat4), REG_FIELD(ExtractedView, frustum, Frustum),
                  REG_FIELD(ExtractedView, viewport, Rect), REG_FIELD(ExtractedView, near_plane, f32),
                  REG_FIELD(ExtractedView, far_plane, f32))

// -- view-resource components (output of culling/queue stages) --

REFLECT_COMPONENT(ViewVisibleList, REG_FIELD(ViewVisibleList, entities, DynamicArray<Entity>))

REFLECT_COMPONENT(ViewBinnedPhases)

REFLECT_COMPONENT(ViewSortedPhases)

// Forces MSVC linker to pull this translation unit from the static library.
// The actual registration happens in the anonymous-namespace constructors above,
// which run before main() as a side effect of loading this .obj.
void registerRenderComponents()
{
    // Intentionally empty — the REFLECT_COMPONENT static initializers above
    // have already run by the time this function is called.
}

} // namespace Entelechy
