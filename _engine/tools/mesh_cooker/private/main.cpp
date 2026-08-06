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
//                 "material":"materials/x.emat"}]}
//
// The 16 transform floats are column-major, matching Mat4::m[16].
// The AABB is the primitive's local-space bounds (same value stored in
// the .emesh header); the game side transforms it into world space for
// frustum culling.
//
// Phase 4a: every glTF material is cooked into materials/<name>.emat
// (hand-emitted JSON: texture content paths + pbrMetallicRoughness
// factors + alphaMode/doubleSided), and the manifest's material field
// references it by manifest-relative path. A primitive without a
// material gets an empty string plus a warning.
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
    // Phase 4a material tallies: alpha/doubleSided numbers scope the
    // pipeline-variant work of Phase 4b.
    usize m_materials_written = 0;
    usize m_materials_mask = 0;
    usize m_materials_blend = 0;
    usize m_materials_double_sided = 0;
    usize m_materials_missing_base_color = 0;
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

// ------------------------------------------------------------------
// Phase 4a: material cooking
// ------------------------------------------------------------------

// File stems must stay portable: replace anything outside
// [A-Za-z0-9_-] with '_'. Falls back to a positional name when the
// glTF material has no name.
std::string materialFileStem(const char *name, usize index)
{
    if (name == nullptr || *name == '\0')
        return "material_" + std::to_string(index);
    std::string out;
    for (const char *p = name; *p != '\0'; ++p)
    {
        const char c = *p;
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                        c == '-';
        out.push_back(ok ? c : '_');
    }
    return out;
}

int hexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// glTF image URIs may be URL-encoded (e.g. %20 for spaces).
std::string percentDecode(const char *uri)
{
    std::string out;
    for (const char *p = uri; *p != '\0'; ++p)
    {
        if (*p == '%' && p[1] != '\0' && p[2] != '\0')
        {
            const int hi = hexDigit(p[1]);
            const int lo = hexDigit(p[2]);
            if (hi >= 0 && lo >= 0)
            {
                out.push_back(static_cast<char>((hi << 4) | lo));
                p += 2;
                continue;
            }
        }
        out.push_back(*p);
    }
    return out;
}

// Resolves a glTF image URI (relative to the .gltf directory) into a
// path relative to the "_content" root (e.g. "sponza/textures/x.png"),
// which is what the engine's VFS "content" mount resolves.
std::string contentPathFromUri(const std::filesystem::path &gltfDir, const char *uri)
{
    const std::filesystem::path joined = (gltfDir / percentDecode(uri)).lexically_normal();
    std::string out;
    bool inContent = false;
    for (const auto &part : joined)
    {
        if (!inContent)
        {
            if (part == "_content")
                inContent = true;
            continue;
        }
        if (!out.empty())
            out.push_back('/');
        out += part.generic_string();
    }
    // Input outside any "_content" root: keep the decoded URI as-is.
    return inContent ? out : percentDecode(uri);
}

const char *textureViewUri(const cgltf_texture_view &view)
{
    if (view.texture != nullptr && view.texture->image != nullptr)
        return view.texture->image->uri;
    return nullptr;
}

const char *alphaModeString(cgltf_alpha_mode mode)
{
    switch (mode)
    {
    case cgltf_alpha_mode_mask:
        return "mask";
    case cgltf_alpha_mode_blend:
        return "blend";
    case cgltf_alpha_mode_opaque:
    default:
        return "opaque";
    }
}

void appendNumber(std::string &out, double value)
{
    char num[32];
    std::snprintf(num, sizeof(num), "%.9g", value);
    out += num;
}

// Writes materials/<name>.emat for every glTF material and returns the
// manifest-relative .emat path per material index ("" on write
// failure). .emat schema (fixed, parsed by MaterialAssetLoader):
//
//   {"base_color_texture":"sponza/textures/x.png",
//    "normal_texture":"...","mr_texture":"...",
//    "base_color_factor":[r,g,b],"metallic_factor":f,
//    "roughness_factor":f,"alpha_mode":"opaque|mask|blend",
//    "alpha_cutoff":f,"double_sided":false}
//
// Texture fields hold content-relative paths ("" when absent); the
// baseColorFactor alpha channel is dropped (engine keeps Vec3).
std::vector<std::string> cookMaterials(const cgltf_data *data, const std::filesystem::path &gltfDir,
                                       const std::filesystem::path &materialsDir, CookStats &stats)
{
    std::vector<std::string> relPaths(data->materials_count);
    for (cgltf_size i = 0; i < data->materials_count; ++i)
    {
        const cgltf_material *mat = &data->materials[i];
        // cgltf zero-fills the struct when the pbr block is absent;
        // restore the glTF spec defaults in that case.
        cgltf_pbr_metallic_roughness pbr = {};
        if (mat->has_pbr_metallic_roughness)
        {
            pbr = mat->pbr_metallic_roughness;
        }
        else
        {
            for (int k = 0; k < 4; ++k)
                pbr.base_color_factor[k] = 1.0f;
            pbr.metallic_factor = 1.0f;
            pbr.roughness_factor = 1.0f;
        }

        if (mat->alpha_mode == cgltf_alpha_mode_mask)
            ++stats.m_materials_mask;
        else if (mat->alpha_mode == cgltf_alpha_mode_blend)
            ++stats.m_materials_blend;
        if (mat->double_sided)
            ++stats.m_materials_double_sided;

        const char *baseColorUri = textureViewUri(pbr.base_color_texture);
        const char *normalUri = textureViewUri(mat->normal_texture);
        const char *mrUri = textureViewUri(pbr.metallic_roughness_texture);
        // Factor-only materials (no baseColor texture) are valid glTF;
        // counted, not warned, to keep the zero-warning regression.
        if (baseColorUri == nullptr)
            ++stats.m_materials_missing_base_color;

        std::string json = "{\"base_color_texture\":\"";
        json += jsonEscape(baseColorUri != nullptr ? contentPathFromUri(gltfDir, baseColorUri).c_str() : "");
        json += "\",\"normal_texture\":\"";
        json += jsonEscape(normalUri != nullptr ? contentPathFromUri(gltfDir, normalUri).c_str() : "");
        json += "\",\"mr_texture\":\"";
        json += jsonEscape(mrUri != nullptr ? contentPathFromUri(gltfDir, mrUri).c_str() : "");
        json += "\",\"base_color_factor\":[";
        for (int k = 0; k < 3; ++k)
        {
            if (k > 0)
                json.push_back(',');
            appendNumber(json, static_cast<double>(pbr.base_color_factor[k]));
        }
        json += "],\"metallic_factor\":";
        appendNumber(json, static_cast<double>(pbr.metallic_factor));
        json += ",\"roughness_factor\":";
        appendNumber(json, static_cast<double>(pbr.roughness_factor));
        json += ",\"alpha_mode\":\"";
        json += alphaModeString(mat->alpha_mode);
        json += "\",\"alpha_cutoff\":";
        appendNumber(json, static_cast<double>(mat->alpha_cutoff));
        json += ",\"double_sided\":";
        json += mat->double_sided ? "true" : "false";
        json += "}\n";

        const std::string fileName = materialFileStem(mat->name, static_cast<usize>(i)) + ".emat";
        const std::string relPath = "materials/" + fileName;
        std::ofstream out(materialsDir / fileName, std::ios::trunc);
        if (!out.is_open())
        {
            warn(stats, "cannot open " + (materialsDir / fileName).string() + " for writing");
            continue;
        }
        out << json;
        if (!out.good())
        {
            warn(stats, "write failed for " + (materialsDir / fileName).string());
            continue;
        }
        relPaths[i] = relPath;
        ++stats.m_materials_written;
    }
    return relPaths;
}

} // namespace

int main(int argc, char **argv)
{
    const char *inputPath = argc > 1 ? argv[1] : DEFAULT_INPUT;
    const std::filesystem::path outputDir = argc > 2 ? argv[2] : DEFAULT_OUTPUT_DIR;
    const std::filesystem::path meshesDir = outputDir / "meshes";
    const std::filesystem::path materialsDir = outputDir / "materials";

    std::error_code ec;
    std::filesystem::create_directories(meshesDir, ec);
    std::filesystem::create_directories(materialsDir, ec);
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

    // Pass 2: cook every glTF material into <out>/materials/*.emat;
    // materialPaths keeps the manifest-relative path per material
    // index for the manifest pass below.
    const std::filesystem::path gltfDir = std::filesystem::path(inputPath).parent_path();
    const std::vector<std::string> materialPaths = cookMaterials(data, gltfDir, materialsDir, stats);

    // Pass 3: walk all nodes; every mesh node contributes one manifest
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
            const char *materialPath = "";
            if (prim->material != nullptr)
            {
                const usize matIndex = static_cast<usize>(prim->material - data->materials);
                materialPath = materialPaths[matIndex].c_str();
            }
            else
            {
                warn(stats, "node " + std::to_string(nodeIndex) + " mesh " + std::to_string(meshIndex) + " prim " +
                                std::to_string(primIndex) + " has no material");
            }

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
            manifest << "],\"material\":\"" << jsonEscape(materialPath) << "\"}";
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
    std::printf("[mesh_cooker] materials: %llu .emat written, %llu mask, %llu blend, %llu doubleSided, %llu "
                "missing baseColor texture\n",
                static_cast<unsigned long long>(stats.m_materials_written),
                static_cast<unsigned long long>(stats.m_materials_mask),
                static_cast<unsigned long long>(stats.m_materials_blend),
                static_cast<unsigned long long>(stats.m_materials_double_sided),
                static_cast<unsigned long long>(stats.m_materials_missing_base_color));
    std::printf("[mesh_cooker] warnings: %llu\n", static_cast<unsigned long long>(stats.m_warnings));

    cgltf_free(data);
    return stats.m_primitives_skipped > 0 ? 2 : 0;
}
