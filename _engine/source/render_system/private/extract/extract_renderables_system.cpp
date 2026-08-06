#include "render_system/extract/extract_renderables_system.h"
#include "render_system/components/mesh_asset_ref.h"
#include "render_system/components/material_asset_ref.h"
#include "render_system/components/world_aabb.h"
#include "ecs/component/transform_component.h"
#include "render_system/components/render_components.h"
#include "ecs/query/query.h"

namespace Entelechy
{

void ExtractRenderablesSystem::extract(const World &mainWorld, World &renderWorld, FrameArena &arena, f32 dt)
{
    m_sync.clear();

    // Query all main-world entities that are renderable.
    ConstQuery<MeshAssetRef, MaterialAssetRef, GlobalTransform> q(mainWorld);
    for (auto [entity, mesh, material, transform] : q)
    {
        Entity renderEntity = renderWorld.spawn();

        renderWorld.addComponent(renderEntity, RenderMesh{mesh->asset_id});
        renderWorld.addComponent(renderEntity, RenderMaterial{material->asset_id});
        renderWorld.addComponent(renderEntity, RenderTransform{transform->matrix});

        // Optional: copy world bounds if present. Entities without a
        // RenderAABB are always visible.
        const WorldAABB *aabb = mainWorld.getComponent<WorldAABB>(entity);
        if (aabb)
        {
            renderWorld.addComponent(renderEntity, RenderAABB{aabb->box});
        }

        m_sync.map(entity, renderEntity);
    }
}

} // namespace Entelechy
