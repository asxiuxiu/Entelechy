#include "render/extract/ExtractRenderablesSystem.h"
#include "render/components/MeshAssetRef.h"
#include "render/components/MaterialAssetRef.h"
#include "ecs/component/transform_component.h"
#include "core/math/aabb.h"
#include "render/components/RenderComponents.h"
#include "ecs/query/query.h"

namespace Entelechy {

void ExtractRenderablesSystem::extract(const World& mainWorld, World& renderWorld, FrameArena& arena, f32 dt) {
    m_sync.clear();

    // Query all main-world entities that are renderable.
    ConstQuery<MeshAssetRef, MaterialAssetRef, GlobalTransform> q(mainWorld);
    for (auto [entity, mesh, material, transform] : q) {
        Entity renderEntity = renderWorld.spawn();

        renderWorld.addComponent(renderEntity, RenderMesh{mesh->asset_id});
        renderWorld.addComponent(renderEntity, RenderMaterial{material->asset_id});
        renderWorld.addComponent(renderEntity, RenderTransform{transform->matrix});

        // Optional: copy AABB if present. Entities without AABB are always visible.
        const AABB* aabb = mainWorld.getComponent<AABB>(entity);
        if (aabb) {
            renderWorld.addComponent(renderEntity, *aabb);
        }

        m_sync.map(entity, renderEntity);
    }
}

} // namespace Entelechy
