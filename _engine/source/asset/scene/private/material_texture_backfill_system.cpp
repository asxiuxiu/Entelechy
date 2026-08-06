// ------------------------------------------------------------------
// material_texture_backfill_system — ECS adapter for SceneLoader
// ------------------------------------------------------------------
#include "scene/material_texture_backfill_system.h"
#include "scene/scene_loader.h"

namespace Entelechy
{

void MaterialTextureBackfillSystem::tick(World &world, FrameArena &arena, f32 dt)
{
    (void)world;
    (void)arena;
    (void)dt;
    m_scene_loader->backfillMaterialTextures();
}

} // namespace Entelechy
