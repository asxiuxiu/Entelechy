// ------------------------------------------------------------------
// scene_loader — cooked scene.json manifest loading (Phase 3c/4b)
// ------------------------------------------------------------------
// Parses the fixed-format manifest emitted by mesh_cooker:
//
//   {"entities":[{"mesh":"meshes/x.emesh","transform":[16 floats],
//                 "aabb_min":[x,y,z],"aabb_max":[x,y,z],
//                 "material":"materials/x.emat"}]}
//
// Every manifest entity becomes one ECS entity with a baked
// GlobalTransform (no local Transform — the propagation system only
// touches entities that have one), an async-loaded MeshAssetRef, its
// own async-loaded MaterialAssetRef (deduplicated by .emat path,
// Phase 4b) and a world-space WorldAABB for culling.
// Parsing uses the shared core JsonCursor (fixed schema, no JSON library).
// ------------------------------------------------------------------
#include "runtime/scene_loader.h"
#include "runtime/render_assets.h"
#include "ecs/world/world.h"
#include "ecs/component/transform_component.h"
#include "render_system/components/MeshAssetRef.h"
#include "render_system/components/MaterialAssetRef.h"
#include "render_system/components/WorldAabb.h"
#include "core/container/hash_map.h"
#include "core/json/json_cursor.h"
#include "core/math/mat4.h"
#include "core/string/string.h"
#include "log/core/log_macros.h"

namespace game
{

namespace
{

using namespace Entelechy;

constexpr LogCategory kLogScene("Scene");

} // namespace

SceneSpawnResult spawnCookedScene(World &world, RenderAssets &assets, const char *scenePath)
{
    SceneSpawnResult result;

    const FileData file = assets.vfs.readFile(Path{scenePath});
    if (!file.valid || file.bytes.size() == 0)
    {
        LOG_ERROR(kLogScene, "SceneLoader: failed to read '%s' (cooker output missing?)", scenePath);
        return result;
    }
    // Null-terminated copy so strtof always stops inside the buffer.
    const String text(reinterpret_cast<const char *>(file.bytes.data()), file.bytes.size());
    JsonCursor cur{text.c_str(), 0, text.length()};

    // Directory prefix for the manifest-relative mesh paths.
    String dir;
    {
        const char *sp = scenePath;
        const char *lastSlash = nullptr;
        for (const char *p = sp; *p != '\0'; ++p)
        {
            if (*p == '/')
                lastSlash = p;
        }
        dir = lastSlash ? String(sp, static_cast<usize>(lastSlash - sp) + 1) : String();
    }

    String key;
    String value;
    const bool headerOk = cur.consume('{') && cur.parseString(key) && key == "entities" && cur.consume(':') &&
                          cur.consume('[');
    if (!headerOk)
    {
        LOG_ERROR(kLogScene, "SceneLoader: '%s' is not a scene manifest", scenePath);
        return result;
    }

    // .emat path -> material handle; one async load per unique material.
    HashMap<String, Handle<MaterialAsset>> materialMap;

    while (!cur.consume(']'))
    {
        String meshRel;
        String materialRel;
        Mat4 matrix = Mat4::identity();
        AABB localBounds{};
        bool entityOk = cur.consume('{');
        while (entityOk)
        {
            if (!cur.parseString(key) || !cur.consume(':'))
                entityOk = false;
            else if (key == "mesh")
                entityOk = cur.parseString(meshRel);
            else if (key == "transform")
                entityOk = cur.parseFloatArray(matrix.m, 16);
            else if (key == "aabb_min")
                entityOk = cur.parseFloatArray(&localBounds.min.x, 3);
            else if (key == "aabb_max")
                entityOk = cur.parseFloatArray(&localBounds.max.x, 3);
            else if (key == "material")
                entityOk = cur.parseString(materialRel);
            else // any future field: skip its value
                entityOk = cur.parseString(value);

            if (!entityOk)
                break;
            if (cur.consume(','))
                continue;
            entityOk = cur.consume('}');
            break;
        }

        if (!entityOk || meshRel.length() == 0)
        {
            LOG_ERROR(kLogScene, "SceneLoader: malformed entity %u in '%s', aborting scene load",
                      result.entity_count, scenePath);
            break;
        }

        String meshPath = dir;
        meshPath += meshRel;
        const Handle<MeshAsset> mesh =
            assets.asset_server.loadAsync(Path{meshPath}, assets.mesh_loader, assets.mesh_assets);

        // Resolve the entity's material, deduplicated by .emat path.
        Handle<MaterialAsset> material;
        {
            String materialPath = dir;
            materialPath += materialRel;
            if (const Handle<MaterialAsset> *cached = materialMap.find(materialPath))
            {
                material = *cached;
            }
            else
            {
                if (materialRel.length() == 0)
                {
                    // Material-less primitive (the cooker already warned on
                    // export): share one default material. Never triggered
                    // for NewSponza (all 405 entities reference a .emat).
                    material = assets.material_assets.insert(MaterialAsset{});
                    LOG_WARN(kLogScene, "SceneLoader: entity %u has no material, using default",
                             result.entity_count);
                }
                else
                {
                    material = assets.asset_server.loadAsync(Path{materialPath}, assets.material_loader,
                                                             assets.material_assets);
                }
                materialMap.insert(materialPath, material);
                assets.scene_materials.pushBack(material);
            }
        }

        const AABB worldBounds = localBounds.transformed(matrix);
        const Entity entity = world.spawn();
        world.addComponent<GlobalTransform>(entity, GlobalTransform{matrix});
        world.addComponent<MeshAssetRef>(entity, MeshAssetRef{mesh});
        world.addComponent<MaterialAssetRef>(entity, MaterialAssetRef{material});
        world.addComponent<WorldAABB>(entity, WorldAABB{worldBounds});

        if (result.entity_count == 0)
            result.world_bounds = worldBounds;
        else
        {
            result.world_bounds.expand(worldBounds.min);
            result.world_bounds.expand(worldBounds.max);
        }
        ++result.entity_count;

        if (cur.consume(','))
            continue;
        cur.consume(']');
        break;
    }

    LOG_INFO(kLogScene,
             "SceneLoader: '%s' -> %u entities spawned, %u unique materials, world bounds "
             "(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)",
             scenePath, result.entity_count, static_cast<u32>(materialMap.size()),
             static_cast<double>(result.world_bounds.min.x), static_cast<double>(result.world_bounds.min.y),
             static_cast<double>(result.world_bounds.min.z), static_cast<double>(result.world_bounds.max.x),
             static_cast<double>(result.world_bounds.max.y), static_cast<double>(result.world_bounds.max.z));
    return result;
}

void MaterialTextureBackfillSystem::tick(World &world, FrameArena &arena, f32 dt)
{
    (void)world;
    (void)arena;
    (void)dt;

    RenderAssets &assets = renderAssets();
    for (usize i = 0; i < assets.scene_materials.size(); ++i)
    {
        MaterialAsset *material = assets.material_assets.get(assets.scene_materials[i]);
        if (material == nullptr)
            continue; // .emat still streaming in
        if (material->base_color_texture_path.length() > 0 && !material->base_color_texture.valid())
        {
            material->base_color_texture = assets.asset_server.loadAsync(Path{material->base_color_texture_path},
                                                                         assets.texture_loader,
                                                                         assets.texture_assets);
            LOG_INFO(kLogScene, "SceneLoader: material %u baseColor texture '%s' load issued",
                     assets.scene_materials[i].index, material->base_color_texture_path.c_str());
        }
    }
}

} // namespace game
