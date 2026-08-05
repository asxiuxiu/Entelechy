#pragma once
#include "asset/type/mesh_asset.h"
#include "core/container/dynamic_array.h"
#include "core/foundation_types.h"
#include <cstring>
#include <type_traits>

namespace Entelechy
{

// ------------------------------------------------------------------
// .emesh — cooked mesh binary format (Phase 3a)
// ------------------------------------------------------------------
// Little-endian binary, written by the offline mesh cooker and read
// back by MeshAssetLoader. Layout:
//
//   MeshFileHeader            (40 bytes)
//   vertexCount * MeshVertex  (interleaved, in-memory layout)
//   indexCount  * u32
//
// No compression, no endianness handling: all supported hosts are
// little-endian and the vertex blob is a raw dump of MeshVertex.
// ------------------------------------------------------------------

inline constexpr u32 EMESH_MAGIC = 0x48534D45u; // "EMSH"
inline constexpr u32 EMESH_VERSION = 1u;

struct MeshFileHeader
{
    u32 magic;
    u32 version;
    u32 vertexCount;
    u32 indexCount;
    Vec3 aabbMin;
    Vec3 aabbMax;
};

STATIC_ASSERT(sizeof(MeshFileHeader) == 40, "MeshFileHeader must be 40 bytes (6 x u32 + 2 x Vec3)");
STATIC_ASSERT(sizeof(MeshVertex) == 12 * sizeof(f32), "MeshVertex must be 12 tightly packed floats");
STATIC_ASSERT(std::is_trivially_copyable_v<MeshVertex>, "MeshVertex must stay trivially copyable for raw dump");

// Serializes a MeshAsset into .emesh bytes. The stored AABB is taken
// from mesh.bounds as-is; callers are expected to computeBounds()
// beforehand (the cooker does). Shared by the mesh cooker and tests.
inline DynamicArray<u8> writeMeshFile(const MeshAsset &mesh)
{
    MeshFileHeader header;
    header.magic = EMESH_MAGIC;
    header.version = EMESH_VERSION;
    header.vertexCount = static_cast<u32>(mesh.vertices.size());
    header.indexCount = static_cast<u32>(mesh.indices.size());
    header.aabbMin = mesh.bounds.min;
    header.aabbMax = mesh.bounds.max;

    const usize vertexBytes = mesh.vertices.size() * sizeof(MeshVertex);
    const usize indexBytes = mesh.indices.size() * sizeof(u32);

    DynamicArray<u8> out;
    out.resize(sizeof(header) + vertexBytes + indexBytes);
    std::memcpy(out.data(), &header, sizeof(header));
    if (vertexBytes > 0)
        std::memcpy(out.data() + sizeof(header), mesh.vertices.data(), vertexBytes);
    if (indexBytes > 0)
        std::memcpy(out.data() + sizeof(header) + vertexBytes, mesh.indices.data(), indexBytes);
    return out;
}

} // namespace Entelechy
