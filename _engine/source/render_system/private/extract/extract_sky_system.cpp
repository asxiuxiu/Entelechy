#include "render_system/extract/extract_sky_system.h"
#include "render_system/components/sky_settings.h"
#include "render_system/components/render_sky.h"
#include "ecs/query/query.h"

namespace Entelechy
{

void ExtractSkySystem::extract(const World &mainWorld, World &renderWorld, FrameArena & /*arena*/, f32 /*dt*/)
{
    ConstQuery<SkySettings> q(mainWorld);
    for (auto [entity, sky] : q)
    {
        (void)entity;

        // The render world is cleared before each extract run, so spawning a
        // fresh entity per frame is correct (same lifetime as the view entity).
        Entity skyEntity = renderWorld.spawn();
        ExtractedSky extracted{};
        extracted.zenith_color = sky->zenith_color;
        extracted.horizon_color = sky->horizon_color;
        extracted.enabled = sky->enabled;
        renderWorld.addComponent(skyEntity, extracted);

        // Only the first sky settings component is extracted.
        break;
    }
}

} // namespace Entelechy
