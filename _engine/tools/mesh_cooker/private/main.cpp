// ------------------------------------------------------------------
// mesh_cooker — offline glTF -> .emesh cook tool (Phase 3b)
// ------------------------------------------------------------------
// Parses a glTF 2.0 scene with cgltf, decodes every unique primitive
// into the interleaved MeshVertex layout and writes one .emesh binary
// per primitive (format: asset/public/type/mesh_format.h). The node
// tree is then walked to bake world transforms into scene.json, a
// hand-emitted JSON manifest consumed by the game-side spawner:
//
//   {"entities":[{"mesh":"meshes/x.emesh","transform":[16 floats],
//                 "aabb_min":[x,y,z],"aabb_max":[x,y,z],
//                 "material":"name placeholder"}]}
//
// The 16 transform floats are column-major, matching Mat4::m[16].
// The AABB is the primitive's local-space bounds (same value stored in
// the .emesh header); the game side transforms it into world space for
// frustum culling. The material field is a placeholder for Phase 4
// (shared white material until then).
//
// Run from the repository root (default paths are relative to cwd):
//   ./build/bin/Debug/MeshCooker.exe [input.gltf] [output_dir]
//
// Cooked output is regenerable and intentionally not committed to git
// (covered by the existing `_content/*` ignore rules).
// ------------------------------------------------------------------
#include "asset/type/mesh_asset.h"
#include "asset/type/mesh_format.h"
#include "core/foundation_types.h"

#include <cgltf.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

using Entelechy::AABB;
using Entelechy::MeshAsset;
using Entelechy::MeshVertex;
using Entelechy::Vec2;
using Entelechy::Vec3;

constexpr const char *DEFAULT_INPUT = "_content/sponza/NewSponza_Main_glTF_003.gltf";
constexpr const char *DEFAULT_OUTPUT_DIR = "_content/sponza/cooked";

// Counters printed at the end for reconciliation against the glTF
// header declarations (Sponza: 115 meshes / 155 nodes / 28 materials).
struct CookStats
{
    usize m_primitives_total = 0;
    usize m_emesh_written = 0;
    usize m_primitives_skipped = 0;
    usize m_entities = 0;
    usize m_warnings = 0;
};

void warn(CookStats &stats, const std::string &message)
{
    std::fprintf(stderr, "[mesh_cooker] WARN: %s\n", message.c_str());
    ++stats.m_warnings;
}

const cgltf_accessor *findAttribute(const cgltf_primitive *prim, cgltf_attribute_type type)
{
    for (cgltf_size i = 0; i < prim->attributes_count; ++i)
    {
        if (prim->attributes[i].type == type)
            return prim->attributes[i].data;
    }
    return nullptr;
}

bool hasSparseAccessor(const cgltf_primitive *prim)
{
    for (cgltf_size i = 0; i < prim->attributes_count; ++i)
    {
        if (prim->attributes[i].data->is_sparse)
            return true;
    }
    return prim->indices != nullptr && prim->indices->is_sparse;
}

std::string meshFileName(usize meshIndex, usize primIndex)
{
    return "mesh_" + std::to_string(meshIndex) + "_prim_" + std::to_string(primIndex) + ".emesh";
}

// Decodes one glTF primitive into MeshAsset and writes it as .emesh.
// Returns false (with a warning) when the primitive cannot be cooked.
bool cookPrimitive(const cgltf_primitive *prim, const std::filesystem::path &outFile, const char *label,
                   CookStats &stats, AABB &outBounds)
{
    if (hasSparseAccessor(prim))
    {
        warn(stats, std::string("skipping ") + label + ": sparse accessors are not supported");
        return false;
    }

    const cgltf_accessor *posAcc = findAttribute(prim, cgltf_attribute_type_position);
    if (posAcc == nullptr)
    {
        warn(stats, std::string("skipping ") + label + ": no POSITION attribute");
        return false;
    }
    const cgltf_accessor *normalAcc = findAttribute(prim, cgltf_attribute_type_normal);
    const cgltf_accessor *uvAcc = findAttribute(prim, cgltf_attribute_type_texcoord);
    const cgltf_accessor *tangentAcc = findAttribute(prim, cgltf_attribute_type_tangent);

    if (normalAcc == nullptr)
        warn(stats, std::string(label) + ": no NORMAL attribute, defaulting to (0,0,1)");
    if (uvAcc == nullptr)
        warn(stats, std::string(label) + ": no TEXCOORD_0 attribute, defaulting to (0,0)");
    if (tangentAcc == nullptr)
        warn(stats, std::string(label) + ": no TANGENT attribute, defaulting to (1,0,0,1)");

    MeshAsset mesh;
    const usize vertexCount = static_cast<usize>(posAcc->count);
    mesh.vertices.resize(vertexCount);

    for (usize i = 0; i < vertexCount; ++i)
    {
        MeshVertex v;
        f32 buf[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        if (cgltf_accessor_read_float(posAcc, i, buf, 3))
            v.position = Vec3{buf[0], buf[1], buf[2]};
        else
            warn(stats, std::string(label) + ": failed to read POSITION, zero-filled");

        if (normalAcc != nullptr && cgltf_accessor_read_float(normalAcc, i, buf, 3))
            v.normal = Vec3{buf[0], buf[1], buf[2]};
        else
            v.normal = Vec3{0.0f, 0.0f, 1.0f};

        if (uvAcc != nullptr && cgltf_accessor_read_float(uvAcc, i, buf, 2))
            v.uv = Vec2{buf[0], buf[1]};

        if (tangentAcc != nullptr && cgltf_accessor_read_float(tangentAcc, i, buf, 4))
        {
            v.tangent = Vec3{buf[0], buf[1], buf[2]};
            v.tangentW = buf[3];
        }
        else
        {
            v.tangent = Vec3{1.0f, 0.0f, 0.0f};
            v.tangentW = 1.0f;
        }

        mesh.vertices[i] = v;
    }

    if (prim->indices != nullptr)
    {
        const usize indexCount = static_cast<usize>(prim->indices->count);
        mesh.indices.resize(indexCount);
        for (usize i = 0; i < indexCount; ++i)
            mesh.indices[i] = static_cast<u32>(cgltf_accessor_read_index(prim->indices, i));
    }
    else
    {
        mesh.indices.resize(vertexCount);
        for (usize i = 0; i < vertexCount; ++i)
            mesh.indices[i] = static_cast<u32>(i);
    }

    mesh.computeBounds();
    outBounds = mesh.bounds;

    Entelechy::DynamicArray<u8> bytes = Entelechy::writeMeshFile(mesh);
    std::ofstream out(outFile, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        warn(stats, std::string("cannot open ") + outFile.string() + " for writing");
        return false;
    }
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out.good())
    {
        warn(stats, std::string("write failed for ") + outFile.string());
        return false;
    }
    return true;
}

// Minimal JSON string escaper (material names come from the asset).
std::string jsonEscape(const char *text)
{
    std::string out;
    if (text == nullptr)
        return out;
    for (const char *p = text; *p != '\0'; ++p)
    {
        if (*p == '"' || *p == '\\')
            out.push_back('\\');
        out.push_back(*p);
    }
    return out;
}

} // namespace

int main(int argc, char **argv)
{
    const char *inputPath = argc > 1 ? argv[1] : DEFAULT_INPUT;
    const std::filesystem::path outputDir = argc > 2 ? argv[2] : DEFAULT_OUTPUT_DIR;
    const std::filesystem::path meshesDir = outputDir / "meshes";

    std::error_code ec;
    std::filesystem::create_directories(meshesDir, ec);
    if (ec)
    {
        std::fprintf(stderr, "[mesh_cooker] ERROR: cannot create output dir '%s': %s\n", meshesDir.string().c_str(),
                     ec.message().c_str());
        return 1;
    }

    cgltf_options options = {};
    cgltf_data *data = nullptr;
    if (cgltf_parse_file(&options, inputPath, &data) != cgltf_result_success)
    {
        std::fprintf(stderr, "[mesh_cooker] ERROR: failed to parse '%s'\n", inputPath);
        return 1;
    }
    if (cgltf_load_buffers(&options, data, inputPath) != cgltf_result_success)
    {
        std::fprintf(stderr, "[mesh_cooker] ERROR: failed to load buffers for '%s'\n", inputPath);
        cgltf_free(data);
        return 1;
    }
    if (cgltf_validate(data) != cgltf_result_success)
    {
        std::fprintf(stderr, "[mesh_cooker] ERROR: glTF validation failed for '%s'\n", inputPath);
        cgltf_free(data);
        return 1;
    }

    CookStats stats;
    std::printf("[mesh_cooker] input:  %s\n", inputPath);
    std::printf("[mesh_cooker] output: %s\n", outputDir.string().c_str());

    // Pass 1: cook every unique primitive into <out>/meshes/*.emesh.
    // cookedFlags tracks per-primitive success so the manifest pass
    // never references a file that was skipped; cookedBounds keeps the
    // local-space AABB for the manifest's aabb_min/aabb_max fields.
    std::vector<std::vector<bool>> cookedFlags(data->meshes_count);
    std::vector<std::vector<AABB>> cookedBounds(data->meshes_count);
    for (cgltf_size meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex)
    {
        const cgltf_mesh *gltfMesh = &data->meshes[meshIndex];
        cookedFlags[meshIndex].resize(gltfMesh->primitives_count, false);
        cookedBounds[meshIndex].resize(gltfMesh->primitives_count);
        for (cgltf_size primIndex = 0; primIndex < gltfMesh->primitives_count; ++primIndex)
        {
            ++stats.m_primitives_total;
            const std::string fileName = meshFileName(meshIndex, primIndex);
            const std::string label = "mesh " + std::to_string(meshIndex) + " prim " + std::to_string(primIndex);
            if (cookPrimitive(&gltfMesh->primitives[primIndex], meshesDir / fileName, label.c_str(), stats,
                              cookedBounds[meshIndex][primIndex]))
            {
                cookedFlags[meshIndex][primIndex] = true;
                ++stats.m_emesh_written;
            }
            else
            {
                ++stats.m_primitives_skipped;
            }
        }
    }

    // Pass 2: walk all nodes; every mesh node contributes one manifest
    // entity per (successfully cooked) primitive with the node world
    // transform baked in (column-major, matches Mat4::m[16]).
    std::ofstream manifest(outputDir / "scene.json", std::ios::trunc);
    if (!manifest.is_open())
    {
        std::fprintf(stderr, "[mesh_cooker] ERROR: cannot open scene.json for writing in '%s'\n",
                     outputDir.string().c_str());
        cgltf_free(data);
        return 1;
    }

    manifest << "{\"entities\":[";
    bool firstEntity = true;
    for (cgltf_size nodeIndex = 0; nodeIndex < data->nodes_count; ++nodeIndex)
    {
        const cgltf_node *node = &data->nodes[nodeIndex];
        if (node->mesh == nullptr)
            continue;

        const usize meshIndex = static_cast<usize>(node->mesh - data->meshes);
        cgltf_float world[16];
        cgltf_node_transform_world(node, world);

        for (cgltf_size primIndex = 0; primIndex < node->mesh->primitives_count; ++primIndex)
        {
            if (!cookedFlags[meshIndex][primIndex])
            {
                warn(stats, "node " + std::to_string(nodeIndex) + " references skipped mesh " +
                                std::to_string(meshIndex) + " prim " + std::to_string(primIndex));
                continue;
            }
            const cgltf_primitive *prim = &node->mesh->primitives[primIndex];
            const char *materialName = prim->material != nullptr ? prim->material->name : nullptr;

            if (!firstEntity)
                manifest << ',';
            firstEntity = false;

            manifest << "{\"mesh\":\"meshes/" << meshFileName(meshIndex, primIndex) << "\",\"transform\":[";
            for (int k = 0; k < 16; ++k)
            {
                char num[32];
                std::snprintf(num, sizeof(num), "%.9g", static_cast<double>(world[k]));
                manifest << (k > 0 ? "," : "") << num;
            }
            const AABB &bounds = cookedBounds[meshIndex][primIndex];
            manifest << "],\"aabb_min\":[";
            for (int k = 0; k < 3; ++k)
            {
                char num[32];
                std::snprintf(num, sizeof(num), "%.9g", static_cast<double>(bounds.min[k]));
                manifest << (k > 0 ? "," : "") << num;
            }
            manifest << "],\"aabb_max\":[";
            for (int k = 0; k < 3; ++k)
            {
                char num[32];
                std::snprintf(num, sizeof(num), "%.9g", static_cast<double>(bounds.max[k]));
                manifest << (k > 0 ? "," : "") << num;
            }
            manifest << "],\"material\":\"" << jsonEscape(materialName) << "\"}";
            ++stats.m_entities;
        }
    }
    manifest << "]}\n";
    manifest.close();

    std::printf("[mesh_cooker] glTF declares: %llu meshes, %llu nodes, %llu materials\n",
                static_cast<unsigned long long>(data->meshes_count),
                static_cast<unsigned long long>(data->nodes_count),
                static_cast<unsigned long long>(data->materials_count));
    std::printf("[mesh_cooker] primitives: %llu total, %llu .emesh written, %llu skipped\n",
                static_cast<unsigned long long>(stats.m_primitives_total),
                static_cast<unsigned long long>(stats.m_emesh_written),
                static_cast<unsigned long long>(stats.m_primitives_skipped));
    std::printf("[mesh_cooker] scene.json entities: %llu\n", static_cast<unsigned long long>(stats.m_entities));
    std::printf("[mesh_cooker] warnings: %llu\n", static_cast<unsigned long long>(stats.m_warnings));

    cgltf_free(data);
    return stats.m_primitives_skipped > 0 ? 2 : 0;
}
