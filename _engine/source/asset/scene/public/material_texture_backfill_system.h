#pragma once
#include "ecs/world/scheduler.h"

namespace Entelechy
{

class World;
class FrameArena;
class SceneLoader;

// ------------------------------------------------------------------
// MaterialTextureBackfillSystem — ECS adapter for the texture backfill
// ------------------------------------------------------------------
// Thin System wrapper so the game can schedule SceneLoader's per-frame
// texture backfill. Touches no ECS components.
// ------------------------------------------------------------------
class MaterialTextureBackfillSystem : public System
{
public:
    explicit MaterialTextureBackfillSystem(SceneLoader &sceneLoader)
        : m_scene_loader(&sceneLoader)
    {
    }

    void tick(World &world, FrameArena &arena, f32 dt) override;

private:
    SceneLoader *m_scene_loader;
};

} // namespace Entelechy
