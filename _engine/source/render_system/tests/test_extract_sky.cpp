#include "test/test_framework.h"
#include "render_system/render_world/RenderWorld.h"
#include "render_system/extract/ExtractSkySystem.h"
#include "render_system/components/SkySettings.h"
#include "render_system/components/RenderSky.h"
#include "ecs/query/query.h"

using namespace Entelechy;

namespace
{

usize countExtractedSkies(World &renderWorld)
{
    usize count = 0;
    ConstQuery<ExtractedSky> q(renderWorld);
    for (auto [entity, sky] : q)
    {
        (void)entity;
        (void)sky;
        ++count;
    }
    return count;
}

const ExtractedSky *firstExtractedSky(World &renderWorld)
{
    ConstQuery<ExtractedSky> q(renderWorld);
    for (auto [entity, sky] : q)
    {
        (void)entity;
        return sky;
    }
    return nullptr;
}

} // namespace

TEST(ExtractSkySystem, CopiesFirstSkySettings)
{
    RenderWorld renderWorld;
    renderWorld.extractSchedule().registerSystem(new ExtractSkySystem());

    World mainWorld;
    Entity sky = mainWorld.spawn();
    mainWorld.addComponent<SkySettings>(sky, SkySettings{{0.2f, 0.4f, 0.8f}, {0.7f, 0.6f, 0.5f}, false});

    renderWorld.extract(mainWorld, 0.016f);

    ASSERT_EQ(countExtractedSkies(renderWorld.world()), 1u);
    const ExtractedSky *extracted = firstExtractedSky(renderWorld.world());
    ASSERT_TRUE(extracted != nullptr);
    ASSERT_EQ(extracted->zenith_color.x, 0.2f);
    ASSERT_EQ(extracted->zenith_color.y, 0.4f);
    ASSERT_EQ(extracted->zenith_color.z, 0.8f);
    ASSERT_EQ(extracted->horizon_color.x, 0.7f);
    ASSERT_EQ(extracted->horizon_color.y, 0.6f);
    ASSERT_EQ(extracted->horizon_color.z, 0.5f);
    ASSERT_TRUE(!extracted->enabled);
}

TEST(ExtractSkySystem, NoSkySettingsNoEntity)
{
    RenderWorld renderWorld;
    renderWorld.extractSchedule().registerSystem(new ExtractSkySystem());

    World mainWorld;
    renderWorld.extract(mainWorld, 0.016f);

    ASSERT_EQ(countExtractedSkies(renderWorld.world()), 0u);
}

TEST(ExtractSkySystem, SecondExtractStillOneSky)
{
    RenderWorld renderWorld;
    renderWorld.extractSchedule().registerSystem(new ExtractSkySystem());

    World mainWorld;
    Entity sky = mainWorld.spawn();
    mainWorld.addComponent<SkySettings>(sky, SkySettings{});

    renderWorld.extract(mainWorld, 0.016f);
    ASSERT_EQ(countExtractedSkies(renderWorld.world()), 1u);

    renderWorld.extract(mainWorld, 0.016f);
    ASSERT_EQ(countExtractedSkies(renderWorld.world()), 1u);
}
