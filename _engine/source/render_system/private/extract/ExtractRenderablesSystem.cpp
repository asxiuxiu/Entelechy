#include "render_system/extract/ExtractRenderablesSystem.h"
#include "render_system/components/MeshAssetRef.h"
#include "render_system/components/MaterialAssetRef.h"
#include "render_system/components/WorldAabb.h"
#include "ecs/component/transform_component.h"
#include "render_system/components/RenderComponents.h"
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
