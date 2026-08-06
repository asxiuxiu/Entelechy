#include "test/test_framework.h"
#include "render_system/render_world/render_world.h"
#include "render_system/extract/extract_light_system.h"
#include "render_system/components/directional_light.h"
#include "render_system/components/render_light.h"
#include "ecs/query/query.h"

using namespace Entelechy;

namespace
{

usize countExtractedLights(World &renderWorld)
{
    usize count = 0;
    ConstQuery<ExtractedLight> q(renderWorld);
    for (auto [entity, light] : q)
    {
        (void)entity;
        (void)light;
        ++count;
    }
    return count;
}

const ExtractedLight *firstExtractedLight(World &renderWorld)
{
    ConstQuery<ExtractedLight> q(renderWorld);
    for (auto [entity, light] : q)
    {
        (void)entity;
        return light;
    }
    return nullptr;
}

} // namespace

TEST(ExtractLightSystem, CopiesFirstDirectionalLight)
{
    RenderWorld renderWorld;
    renderWorld.extractSchedule().registerSystem(new ExtractLightSystem());

    World mainWorld;
    Entity sun = mainWorld.spawn();
    mainWorld.addComponent<DirectionalLight>(
        sun, DirectionalLight{{0.0f, -2.0f, 0.0f}, {1.0f, 0.9f, 0.8f}, 2.5f, 0.05f});

    renderWorld.extract(mainWorld, 0.016f);

    ASSERT_EQ(countExtractedLights(renderWorld.world()), 1u);
    const ExtractedLight *light = firstExtractedLight(renderWorld.world());
    ASSERT_TRUE(light != nullptr);
    // Direction is normalized during extract.
    ASSERT_TRUE(light->direction.y < -0.99f && light->direction.y > -1.01f);
    ASSERT_EQ(light->direction.x, 0.0f);
    ASSERT_EQ(light->direction.z, 0.0f);
    // Color/intensity/ambient are copied verbatim.
    ASSERT_EQ(light->color.x, 1.0f);
    ASSERT_EQ(light->color.y, 0.9f);
    ASSERT_EQ(light->color.z, 0.8f);
    ASSERT_EQ(light->intensity, 2.5f);
    ASSERT_EQ(light->ambient, 0.05f);
}

TEST(ExtractLightSystem, NoLightNoEntity)
{
    RenderWorld renderWorld;
    renderWorld.extractSchedule().registerSystem(new ExtractLightSystem());

    World mainWorld;
    renderWorld.extract(mainWorld, 0.016f);

    ASSERT_EQ(countExtractedLights(renderWorld.world()), 0u);
}

TEST(ExtractLightSystem, SecondExtractStillOneLight)
{
    RenderWorld renderWorld;
    renderWorld.extractSchedule().registerSystem(new ExtractLightSystem());

    World mainWorld;
    Entity sun = mainWorld.spawn();
    mainWorld.addComponent<DirectionalLight>(sun, DirectionalLight{});

    renderWorld.extract(mainWorld, 0.016f);
    ASSERT_EQ(countExtractedLights(renderWorld.world()), 1u);

    renderWorld.extract(mainWorld, 0.016f);
    ASSERT_EQ(countExtractedLights(renderWorld.world()), 1u);
}

TEST(ExtractLightSystem, TakesFirstLightOnly)
{
    RenderWorld renderWorld;
    renderWorld.extractSchedule().registerSystem(new ExtractLightSystem());

    World mainWorld;
    Entity sunA = mainWorld.spawn();
    mainWorld.addComponent<DirectionalLight>(sunA, DirectionalLight{{0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 3.0f, 0.03f});
    Entity sunB = mainWorld.spawn();
    mainWorld.addComponent<DirectionalLight>(sunB, DirectionalLight{{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 9.0f, 0.5f});

    renderWorld.extract(mainWorld, 0.016f);

    ASSERT_EQ(countExtractedLights(renderWorld.world()), 1u);
}

TEST(ExtractLightSystem, ZeroDirectionFallsBackToDown)
{
    RenderWorld renderWorld;
    renderWorld.extractSchedule().registerSystem(new ExtractLightSystem());

    World mainWorld;
    Entity sun = mainWorld.spawn();
    mainWorld.addComponent<DirectionalLight>(sun, DirectionalLight{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 1.0f, 0.03f});

    renderWorld.extract(mainWorld, 0.016f);

    const ExtractedLight *light = firstExtractedLight(renderWorld.world());
    ASSERT_TRUE(light != nullptr);
    ASSERT_EQ(light->direction.x, 0.0f);
    ASSERT_EQ(light->direction.y, -1.0f);
    ASSERT_EQ(light->direction.z, 0.0f);
}
