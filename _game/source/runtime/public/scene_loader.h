#pragma once
#include "core/foundation_types.h"
#include "core/math/aabb.h"
#include "ecs/world/scheduler.h"

namespace Entelechy
{
class World;
}

namespace game
{

struct RenderAssets;

// Summary of a spawned cooked scene, returned for logging and camera
// placement.
struct SceneSpawnResult
{
    u32 entity_count = 0;
    Entelechy::AABB world_bounds{};
};

// Reads a mesh_cooker scene.json manifest through the RenderAssets VFS,
// kicks off one async .emesh load per entity and one async .emat load
// per unique material path, and spawns one ECS entity per manifest
// entry: GlobalTransform (baked world matrix, no local Transform),
// MeshAssetRef, its own MaterialAssetRef (Phase 4b) and a WorldAABB
// (local bounds x world matrix, for frustum culling).
// Unique material handles are recorded in RenderAssets::scene_materials
// for MaterialTextureBackfillSystem.
// Returns an empty result on failure (logged).
SceneSpawnResult spawnCookedScene(Entelechy::World &world, RenderAssets &assets, const char *scenePath);

// ------------------------------------------------------------------
// MaterialTextureBackfillSystem — texture handle backfill (Phase 4b)
// ------------------------------------------------------------------
// .emat files load asynchronously and only carry texture *paths*
// (MaterialAssetLoader never triggers texture loads, D1). Each frame
// this system scans the scene materials recorded by spawnCookedScene;
// once a material's data has landed, it loadAsync's the baseColor
// texture path and back-fills the Handle. Until then the Prepare stage
// keeps the material on the pink fallback; once the texture lands the
// existing prepare/retry path picks it up.
// Normal/MR textures are deliberately not loaded (D4, Phase 4c).
// The per-frame polling scan is a transitional mechanism — see TODO.md.
// ------------------------------------------------------------------
class MaterialTextureBackfillSystem : public Entelechy::System
{
public:
    void tick(Entelechy::World &world, Entelechy::FrameArena &arena, f32 dt) override;
};

} // namespace game
