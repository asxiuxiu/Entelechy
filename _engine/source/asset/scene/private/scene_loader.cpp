// ------------------------------------------------------------------
// scene_loader — cooked scene.json manifest loading
// ------------------------------------------------------------------
// Engine home of the mesh_cooker scene manifest consumer (moved from
// the game side). See the header for the manifest schema and the
// injection contract.
// ------------------------------------------------------------------
#include "scene/scene_loader.h"
#include "ecs/world/world.h"
#include "ecs/component/transform_component.h"
#include "render_system/components/mesh_asset_ref.h"
#include "render_system/components/material_asset_ref.h"
#include "render_system/components/world_aabb.h"
#include "core/container/hash_map.h"
#include "core/json/json_cursor.h"
#include "core/math/mat4.h"
#include "core/string/string.h"
#include "log/core/log_macros.h"

namespace Entelechy
{

namespace
{

constexpr LogCategory kLogScene("Scene");

} // namespace

SceneLoader::SceneLoader(VFS &vfs, AssetServer &assetServer, MeshAssetLoader &meshLoader,
                         TextureAssetLoader &textureLoader, Assets<MeshAsset> &meshAssets,
                         Assets<MaterialAsset> &materialAssets, Assets<TextureAsset> &textureAssets)
    : m_vfs(&vfs)
    , m_asset_server(&assetServer)
    , m_mesh_loader(&meshLoader)
    , m_texture_loader(&textureLoader)
    , m_mesh_assets(&meshAssets)
    , m_material_assets(&materialAssets)
    , m_texture_assets(&textureAssets)
{
}

SceneSpawnResult SceneLoader::spawnCookedScene(World &world, const char *scenePath)
{
    SceneSpawnResult result;

    const FileData file = m_vfs->readFile(Path{scenePath});
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
            m_asset_server->loadAsync(Path{meshPath}, *m_mesh_loader, *m_mesh_assets);

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
                    material = m_material_assets->insert(MaterialAsset{});
                    LOG_WARN(kLogScene, "SceneLoader: entity %u has no material, using default",
                             result.entity_count);
                }
                else
                {
                    material = m_asset_server->loadAsync(Path{materialPath}, m_material_loader,
                                                         *m_material_assets);
                }
                materialMap.insert(materialPath, material);
                m_scene_materials.pushBack(material);
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

void SceneLoader::backfillMaterialTextures()
{
    for (usize i = 0; i < m_scene_materials.size(); ++i)
    {
        MaterialAsset *material = m_material_assets->get(m_scene_materials[i]);
        if (material == nullptr)
            continue; // .emat still streaming in

        // One async load per texture path whose Handle is not yet
        // back-filled. Normal/MR are loaded for the lighting phase:
        // the data lands in Assets<TextureAsset> and the Handle in the
        // material, but Prepare never binds them (no shader consumer).
        const u32 materialIndex = m_scene_materials[i].index;
        auto backfill = [this, materialIndex](String &texturePath, Handle<TextureAsset> &textureHandle,
                                              const char *label)
        {
            if (texturePath.length() > 0 && !textureHandle.valid())
            {
                textureHandle =
                    m_asset_server->loadAsync(Path{texturePath}, *m_texture_loader, *m_texture_assets);
                LOG_INFO(kLogScene, "SceneLoader: material %u %s texture '%s' load issued", materialIndex,
                         label, texturePath.c_str());
            }
        };
        backfill(material->base_color_texture_path, material->base_color_texture, "baseColor");
        backfill(material->normal_texture_path, material->normal_texture, "normal");
        backfill(material->mr_texture_path, material->mr_texture, "MR");
    }
}

} // namespace Entelechy
