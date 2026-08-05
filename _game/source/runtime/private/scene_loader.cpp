// ------------------------------------------------------------------
// scene_loader — cooked scene.json manifest loading (Phase 3c)
// ------------------------------------------------------------------
// Parses the fixed-format manifest emitted by mesh_cooker:
//
//   {"entities":[{"mesh":"meshes/x.emesh","transform":[16 floats],
//                 "aabb_min":[x,y,z],"aabb_max":[x,y,z],
//                 "material":"name placeholder"}]}
//
// Every manifest entity becomes one ECS entity with a baked
// GlobalTransform (no local Transform — the propagation system only
// touches entities that have one), an async-loaded MeshAssetRef, the
// shared white-model material and a world-space AABB for culling.
// The parser is a purpose-built cursor for this exact schema (same
// style as the ECS SceneSerializer's JsonCursor); no JSON library.
// ------------------------------------------------------------------
#include "runtime/scene_loader.h"
#include "runtime/render_assets.h"
#include "ecs/world/world.h"
#include "ecs/component/transform_component.h"
#include "render_system/components/MeshAssetRef.h"
#include "render_system/components/MaterialAssetRef.h"
#include "core/math/mat4.h"
#include "core/string/string.h"
#include "log/core/log_macros.h"
#include <cstdlib>

namespace game
{

namespace
{

using namespace Entelechy;

constexpr LogCategory kLogScene("Scene");

// Minimal cursor over the manifest text; modeled on the ECS
// SceneSerializer's JsonCursor (which is file-local and cannot be
// reused directly).
struct ManifestCursor
{
    const char *s;
    usize pos;
    usize len;

    void skipWs()
    {
        while (pos < len && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
            ++pos;
    }

    bool consume(char c)
    {
        skipWs();
        if (pos < len && s[pos] == c)
        {
            ++pos;
            return true;
        }
        return false;
    }

    bool parseString(String &out)
    {
        skipWs();
        if (pos >= len || s[pos] != '"')
            return false;
        ++pos;
        usize start = pos;
        while (pos < len && s[pos] != '"')
        {
            if (s[pos] == '\\' && pos + 1 < len)
                pos += 2;
            else
                ++pos;
        }
        if (pos >= len)
            return false;
        out = String(s + start, pos - start);
        ++pos; // skip closing quote
        return true;
    }

    bool parseFloat(f32 &out)
    {
        skipWs();
        if (pos >= len)
            return false;
        char *end = nullptr;
        out = std::strtof(s + pos, &end);
        if (end == s + pos)
            return false;
        pos = static_cast<usize>(end - s);
        return true;
    }

    bool parseFloatArray(f32 *out, usize count)
    {
        if (!consume('['))
            return false;
        for (usize i = 0; i < count; ++i)
        {
            if (i > 0 && !consume(','))
                return false;
            if (!parseFloat(out[i]))
                return false;
        }
        return consume(']');
    }
};

// World-space AABB = the 8 transformed corners of the local box.
AABB transformAabb(const Mat4 &m, const AABB &local)
{
    AABB out = AABB::fromMinMax(m.transformPoint(local.min), m.transformPoint(local.min));
    for (u32 i = 0; i < 8; ++i)
    {
        const Vec3 corner{(i & 1u) ? local.max.x : local.min.x, (i & 2u) ? local.max.y : local.min.y,
                          (i & 4u) ? local.max.z : local.min.z};
        out.expand(m.transformPoint(corner));
    }
    return out;
}

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
    ManifestCursor cur{text.c_str(), 0, text.length()};

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

    while (!cur.consume(']'))
    {
        String meshRel;
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
            else // "material" placeholder and any future field: skip its value
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

        const AABB worldBounds = transformAabb(matrix, localBounds);
        const Entity entity = world.spawn();
        world.addComponent<GlobalTransform>(entity, GlobalTransform{matrix});
        world.addComponent<MeshAssetRef>(entity, MeshAssetRef{mesh});
        world.addComponent<MaterialAssetRef>(entity, MaterialAssetRef{assets.mat_white});
        world.addComponent<AABB>(entity, worldBounds);

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
             "SceneLoader: '%s' -> %u entities spawned, world bounds (%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)", scenePath,
             result.entity_count, static_cast<double>(result.world_bounds.min.x),
             static_cast<double>(result.world_bounds.min.y), static_cast<double>(result.world_bounds.min.z),
             static_cast<double>(result.world_bounds.max.x), static_cast<double>(result.world_bounds.max.y),
             static_cast<double>(result.world_bounds.max.z));
    return result;
}

} // namespace game
