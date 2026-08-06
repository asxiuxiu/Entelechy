#pragma once
#include "asset/handle/asset_handle.h"
#include "asset/loader/asset_server.h"
#include "asset/loader/material_asset_loader.h"
#include "asset/loader/mesh_asset_loader.h"
#include "asset/loader/texture_asset_loader.h"
#include "asset/type/assets.h"
#include "asset/type/material_asset.h"
#include "asset/type/mesh_asset.h"
#include "asset/type/texture_asset.h"
#include "core/container/dynamic_array.h"
#include "core/foundation_types.h"
#include "core/math/aabb.h"
#include "ecs/world/scheduler.h"

namespace Entelechy
{

class World;

// Summary of a spawned cooked scene, returned for logging and camera
// placement.
struct SceneSpawnResult
{
    u32 entity_count = 0;
    AABB world_bounds{};
};

// ------------------------------------------------------------------
// SceneLoader — cooked scene.json manifest loading (Phase 4c, D5)
// ------------------------------------------------------------------
// Parses the fixed-format manifest emitted by mesh_cooker:
//
//   {"entities":[{"mesh":"meshes/x.emesh","transform":[16 floats],
//                 "aabb_min":[x,y,z],"aabb_max":[x,y,z],
//                 "material":"materials/x.emat"}]}
//
// scene.json and .emat are both engine-owned formats (mesh_cooker is
// an engine tool), so the loader lives in the engine (moved here from
// the game side in Phase 4c). Every manifest entity becomes one ECS
// entity with a baked GlobalTransform (no local Transform — the
// propagation system only touches entities that have one), an
// async-loaded MeshAssetRef, its own async-loaded MaterialAssetRef
// (deduplicated by .emat path, Phase 4b) and a world-space WorldAABB
// for culling. Parsing uses the shared core JsonCursor (fixed schema,
// no JSON library).
//
// All collaborators (VFS / AssetServer / loaders / Assets<T> storages)
// are injected by the caller — the engine holds no game-side globals.
// The MaterialAssetLoader is owned internally: .emat files are only
// ever consumed through this loader. Unique material handles are kept
// in m_scene_materials for backfillMaterialTextures() (Assets<T>
// supports no iteration, so a separate list is the simplest option).
// ------------------------------------------------------------------
class SceneLoader
{
public:
    SceneLoader(VFS &vfs, AssetServer &assetServer, MeshAssetLoader &meshLoader,
                TextureAssetLoader &textureLoader, Assets<MeshAsset> &meshAssets,
                Assets<MaterialAsset> &materialAssets, Assets<TextureAsset> &textureAssets);

    // Reads a mesh_cooker scene.json manifest through the injected VFS,
    // kicks off one async .emesh load per entity and one async .emat
    // load per unique material path, and spawns one ECS entity per
    // manifest entry. Returns an empty result on failure (logged).
    SceneSpawnResult spawnCookedScene(World &world, const char *scenePath);

    // ------------------------------------------------------------------
    // Texture handle backfill (Phase 4b/4c)
    // ------------------------------------------------------------------
    // .emat files load asynchronously and only carry texture *paths*
    // (MaterialAssetLoader never triggers texture loads, D1). Called
    // once per frame (via MaterialTextureBackfillSystem); scans the
    // scene materials recorded by spawnCookedScene and, once a
    // material's data has landed, loadAsync's each texture path and
    // back-fills the Handle. Until then the Prepare stage keeps the
    // material on the pink fallback; once the baseColor texture lands
    // the existing prepare/retry path picks it up.
    // Normal/MR textures are loaded and back-filled the same way but
    // are never sampled by the shader (D4 — no consumer until the
    // lighting phase).
    // The per-frame polling scan is a transitional mechanism — see
    // TODO.md.
    // ------------------------------------------------------------------
    void backfillMaterialTextures();

private:
    VFS *m_vfs;
    AssetServer *m_asset_server;
    MeshAssetLoader *m_mesh_loader;
    TextureAssetLoader *m_texture_loader;
    Assets<MeshAsset> *m_mesh_assets;
    Assets<MaterialAsset> *m_material_assets;
    Assets<TextureAsset> *m_texture_assets;

    MaterialAssetLoader m_material_loader;
    DynamicArray<Handle<MaterialAsset>> m_scene_materials;
};

// ------------------------------------------------------------------
// MaterialTextureBackfillSystem — ECS adapter for the backfill
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
