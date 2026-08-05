#pragma once
#include "core/foundation_types.h"
#include "core/math/aabb.h"

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
// kicks off one async .emesh load per entity and spawns one ECS entity
// per manifest entry: GlobalTransform (baked world matrix, no local
// Transform), MeshAssetRef, the shared white-model MaterialAssetRef and
// a WorldAABB (local bounds x world matrix, for frustum culling).
// Returns an empty result on failure (logged).
SceneSpawnResult spawnCookedScene(Entelechy::World &world, RenderAssets &assets, const char *scenePath);

} // namespace game
