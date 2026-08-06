#include "render_system/extract/ExtractLightSystem.h"
#include "render_system/components/DirectionalLight.h"
#include "render_system/components/RenderLight.h"
#include "ecs/query/query.h"

namespace Entelechy
{

void ExtractLightSystem::extract(const World &mainWorld, World &renderWorld, FrameArena & /*arena*/, f32 /*dt*/)
{
    ConstQuery<DirectionalLight> q(mainWorld);
    for (auto [entity, light] : q)
    {
        (void)entity;

        // The render world is cleared before each extract run, so spawning a
        // fresh entity per frame is correct (same lifetime as the view entity).
        Entity lightEntity = renderWorld.spawn();
        ExtractedLight extracted{};
        extracted.direction = light->direction.dot(light->direction) > 1e-8f ? light->direction.normalized()
                                                                       : Vec3{0.0f, -1.0f, 0.0f};
        extracted.color = light->color;
        extracted.intensity = light->intensity;
        extracted.ambient = light->ambient;
        renderWorld.addComponent(lightEntity, extracted);

        // Phase 5a: only the first directional light is extracted.
        break;
    }
}

} // namespace Entelechy
