#include "asset/loader/mesh_asset_loader.h"
#include "asset/type/mesh_format.h"
#include "log/core/log_macros.h"
#include <cstring>

namespace Entelechy
{

namespace
{
constexpr LogCategory kLogAsset("Asset");
}

MeshAsset MeshAssetLoader::load(const FileData &data, const Path &path)
{
    MeshAsset mesh;
    if (!data.valid || data.bytes.size() < sizeof(MeshFileHeader))
    {
        LOG_ERROR(kLogAsset, "MeshAssetLoader: empty or too small file data for '%s'", path.c_str());
        return mesh;
    }

    MeshFileHeader header;
    std::memcpy(&header, data.bytes.data(), sizeof(header));

    if (header.magic != EMESH_MAGIC)
    {
        LOG_ERROR(kLogAsset, "MeshAssetLoader: bad magic in '%s' (not an .emesh file)", path.c_str());
        return mesh;
    }
    if (header.version != EMESH_VERSION)
    {
        LOG_ERROR(kLogAsset, "MeshAssetLoader: unsupported .emesh version %u in '%s' (expected %u)", header.version,
                  path.c_str(), EMESH_VERSION);
        return mesh;
    }

    const usize vertexBytes = static_cast<usize>(header.vertexCount) * sizeof(MeshVertex);
    const usize indexBytes = static_cast<usize>(header.indexCount) * sizeof(u32);
    const usize expected = sizeof(MeshFileHeader) + vertexBytes + indexBytes;
    if (expected != data.bytes.size())
    {
        LOG_ERROR(kLogAsset, "MeshAssetLoader: size mismatch in '%s' (header declares %llu bytes, file has %llu)",
                  path.c_str(), static_cast<unsigned long long>(expected),
                  static_cast<unsigned long long>(data.bytes.size()));
        return mesh;
    }

    mesh.vertices.resize(header.vertexCount);
    mesh.indices.resize(header.indexCount);
    if (vertexBytes > 0)
        std::memcpy(mesh.vertices.data(), data.bytes.data() + sizeof(MeshFileHeader), vertexBytes);
    if (indexBytes > 0)
        std::memcpy(mesh.indices.data(), data.bytes.data() + sizeof(MeshFileHeader) + vertexBytes, indexBytes);
    mesh.bounds = AABB::fromMinMax(header.aabbMin, header.aabbMax);
    return mesh;
}

} // namespace Entelechy
