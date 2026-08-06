// component_registration.cpp — centralized ECS component registration for all
// render_system components. REFLECT_COMPONENT calls were relocated here to keep
// component headers focused on data definitions.
//
// Every REFLECT_COMPONENT lives in exactly one .obj inside RenderSystemLib.
// registerRenderComponents() is a no-op function whose sole purpose is to force
// MSVC's linker to pull this translation unit from RenderSystemLib.lib; the
// anonymous-namespace static initializers run before main() as a side effect.

#include "render_system/components/camera.h"
#include "render_system/components/directional_light.h"
#include "render_system/components/material_asset_ref.h"
#include "render_system/components/mesh_asset_ref.h"
#include "render_system/components/render_camera.h"
#include "render_system/components/render_components.h"
#include "render_system/components/render_light.h"
#include "render_system/components/render_sky.h"
#include "render_system/components/sky_settings.h"
#include "render_system/components/world_aabb.h"
#include "render_system/culling/view_visible_list.h"
#include "render_system/phase/render_resources.h"
#include "ecs/type/type_registry.h"

namespace Entelechy
{

// -- main-world components (used by extract systems) --

REFLECT_COMPONENT(Camera, REG_FIELD(Camera, fov_y, f32), REG_FIELD(Camera, near_plane, f32),
                  REG_FIELD(Camera, far_plane, f32), REG_FIELD(Camera, orthographic, bool),
                  REG_FIELD(Camera, ortho_size, f32))

// NOTE: Handle<T> fields are not reflected. REG_FIELD only
// addresses flat members and stringifies the field type, so a nested
// Handle<T> (or its index/generation pair) cannot be registered with the
// current macros. These components are registered without fields until the
// reflection system learns to decompose Handle<T>.
REFLECT_COMPONENT(MeshAssetRef)

REFLECT_COMPONENT(MaterialAssetRef)

// NOTE: registered without fields — REG_FIELD only addresses flat
// members, so the nested AABB (and its Vec3s) cannot be decomposed by
// the current reflection macros.
REFLECT_COMPONENT(WorldAABB)

REFLECT_COMPONENT(DirectionalLight, REG_FIELD(DirectionalLight, direction, Vec3),
                  REG_FIELD(DirectionalLight, color, Vec3), REG_FIELD(DirectionalLight, intensity, f32),
                  REG_FIELD(DirectionalLight, ambient, f32))

REFLECT_COMPONENT(SkySettings, REG_FIELD(SkySettings, zenith_color, Vec3),
                  REG_FIELD(SkySettings, horizon_color, Vec3), REG_FIELD(SkySettings, enabled, bool))

// -- render-world components (spawned/extracted by systems) --

REFLECT_COMPONENT(RenderMesh)

REFLECT_COMPONENT(RenderMaterial, REG_FIELD(RenderMaterial, render_phase, RenderPhase))

REFLECT_COMPONENT(RenderTransform, REG_FIELD(RenderTransform, world_matrix, Mat4))

REFLECT_COMPONENT(RenderAABB)

REFLECT_COMPONENT(ExtractedView, REG_FIELD(ExtractedView, view_matrix, Mat4),
                  REG_FIELD(ExtractedView, proj_matrix, Mat4), REG_FIELD(ExtractedView, frustum, Frustum),
                  REG_FIELD(ExtractedView, viewport, Rect), REG_FIELD(ExtractedView, near_plane, f32),
                  REG_FIELD(ExtractedView, far_plane, f32), REG_FIELD(ExtractedView, view_pos, Vec3))

REFLECT_COMPONENT(ExtractedLight, REG_FIELD(ExtractedLight, direction, Vec3),
                  REG_FIELD(ExtractedLight, color, Vec3), REG_FIELD(ExtractedLight, intensity, f32),
                  REG_FIELD(ExtractedLight, ambient, f32))

REFLECT_COMPONENT(ExtractedSky, REG_FIELD(ExtractedSky, zenith_color, Vec3),
                  REG_FIELD(ExtractedSky, horizon_color, Vec3), REG_FIELD(ExtractedSky, enabled, bool))

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
