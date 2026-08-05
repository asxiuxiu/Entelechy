#include "test/test_framework.h"
#include "asset/loader/mesh_asset_loader.h"
#include "asset/type/mesh_asset.h"
#include "asset/type/mesh_format.h"
#include <cstring>

// ------------------------------------------------------------------
// MeshAssetLoader / .emesh format tests
// ------------------------------------------------------------------
namespace
{

Entelechy::MeshAsset makeTestMesh()
{
    Entelechy::MeshAsset mesh;
    mesh.vertices.pushBack(Entelechy::MeshVertex{.position = {-1.0f, 2.0f, 3.0f},
                                                 .normal = {0.0f, 1.0f, 0.0f},
                                                 .uv = {0.25f, 0.75f},
                                                 .tangent = {1.0f, 0.0f, 0.0f},
                                                 .tangentW = -1.0f});
    mesh.vertices.pushBack(Entelechy::MeshVertex{.position = {4.0f, -5.0f, 6.0f},
                                                 .normal = {0.0f, 0.0f, 1.0f},
                                                 .uv = {1.0f, 0.0f},
                                                 .tangent = {0.0f, 1.0f, 0.0f},
                                                 .tangentW = 1.0f});
    mesh.vertices.pushBack(Entelechy::MeshVertex{.position = {-7.0f, 8.0f, -9.0f},
                                                 .normal = {1.0f, 0.0f, 0.0f},
                                                 .uv = {0.5f, 0.5f},
                                                 .tangent = {0.0f, 0.0f, -1.0f},
                                                 .tangentW = -1.0f});
    mesh.indices.pushBack(0u);
    mesh.indices.pushBack(1u);
    mesh.indices.pushBack(2u);
    mesh.computeBounds();
    return mesh;
}

Entelechy::FileData makeFileData(const Entelechy::DynamicArray<u8> &bytes)
{
    Entelechy::FileData data;
    data.bytes.resize(bytes.size());
    if (bytes.size() > 0)
        std::memcpy(data.bytes.data(), bytes.data(), bytes.size());
    data.valid = true;
    return data;
}

} // namespace

TEST(Asset, MeshLoaderRoundTrip)
{
    const Entelechy::MeshAsset source = makeTestMesh();
    const Entelechy::DynamicArray<u8> bytes = Entelechy::writeMeshFile(source);

    Entelechy::MeshAssetLoader loader;
    const Entelechy::MeshAsset loaded = loader.load(makeFileData(bytes), Entelechy::Path{"test.emesh"});

    ASSERT_EQ(loaded.vertices.size(), source.vertices.size());
    ASSERT_EQ(loaded.indices.size(), source.indices.size());
    // Bit-exact round trip: raw vertex/index blobs are memcmp-comparable.
    ASSERT_EQ(std::memcmp(loaded.vertices.data(), source.vertices.data(),
                          source.vertices.size() * sizeof(Entelechy::MeshVertex)),
              0);
    ASSERT_EQ(std::memcmp(loaded.indices.data(), source.indices.data(), source.indices.size() * sizeof(u32)), 0);

    ASSERT_EQ(loaded.bounds.min.x, source.bounds.min.x);
    ASSERT_EQ(loaded.bounds.min.y, source.bounds.min.y);
    ASSERT_EQ(loaded.bounds.min.z, source.bounds.min.z);
    ASSERT_EQ(loaded.bounds.max.x, source.bounds.max.x);
    ASSERT_EQ(loaded.bounds.max.y, source.bounds.max.y);
    ASSERT_EQ(loaded.bounds.max.z, source.bounds.max.z);
}

TEST(Asset, MeshLoaderRoundTripEmpty)
{
    // A zero-vertex file is well-formed and loads as an empty mesh.
    Entelechy::MeshAsset source;
    source.computeBounds();
    const Entelechy::DynamicArray<u8> bytes = Entelechy::writeMeshFile(source);
    ASSERT_EQ(bytes.size(), sizeof(Entelechy::MeshFileHeader));

    Entelechy::MeshAssetLoader loader;
    const Entelechy::MeshAsset loaded = loader.load(makeFileData(bytes), Entelechy::Path{"empty.emesh"});
    ASSERT_EQ(loaded.vertices.size(), 0u);
    ASSERT_EQ(loaded.indices.size(), 0u);
}

TEST(Asset, MeshLoaderRejectsGarbage)
{
    const Entelechy::DynamicArray<u8> garbage = [] {
        Entelechy::DynamicArray<u8> bytes;
        for (u8 i = 0; i < 64; ++i)
            bytes.pushBack(static_cast<u8>(i * 37u + 1u));
        return bytes;
    }();

    Entelechy::MeshAssetLoader loader;
    const Entelechy::MeshAsset loaded = loader.load(makeFileData(garbage), Entelechy::Path{"bad.emesh"});
    ASSERT_EQ(loaded.vertices.size(), 0u);
    ASSERT_EQ(loaded.indices.size(), 0u);
}

TEST(Asset, MeshLoaderRejectsTruncated)
{
    Entelechy::DynamicArray<u8> bytes = Entelechy::writeMeshFile(makeTestMesh());
    // Cut the file mid-index-array.
    bytes.resize(bytes.size() - 3);

    Entelechy::MeshAssetLoader loader;
    const Entelechy::MeshAsset loaded = loader.load(makeFileData(bytes), Entelechy::Path{"truncated.emesh"});
    ASSERT_EQ(loaded.vertices.size(), 0u);
    ASSERT_EQ(loaded.indices.size(), 0u);
}

TEST(Asset, MeshLoaderRejectsBadMagic)
{
    Entelechy::DynamicArray<u8> bytes = Entelechy::writeMeshFile(makeTestMesh());
    bytes[0] ^= 0xFF;

    Entelechy::MeshAssetLoader loader;
    const Entelechy::MeshAsset loaded = loader.load(makeFileData(bytes), Entelechy::Path{"magic.emesh"});
    ASSERT_EQ(loaded.vertices.size(), 0u);
}

TEST(Asset, MeshLoaderRejectsBadVersion)
{
    Entelechy::DynamicArray<u8> bytes = Entelechy::writeMeshFile(makeTestMesh());
    const u32 badVersion = Entelechy::EMESH_VERSION + 1;
    std::memcpy(bytes.data() + offsetof(Entelechy::MeshFileHeader, version), &badVersion, sizeof(badVersion));

    Entelechy::MeshAssetLoader loader;
    const Entelechy::MeshAsset loaded = loader.load(makeFileData(bytes), Entelechy::Path{"version.emesh"});
    ASSERT_EQ(loaded.vertices.size(), 0u);
}

TEST(Asset, MeshLoaderRejectsInvalidFileData)
{
    Entelechy::MeshAssetLoader loader;
    const Entelechy::MeshAsset loaded = loader.load(Entelechy::FileData{}, Entelechy::Path{"missing.emesh"});
    ASSERT_EQ(loaded.vertices.size(), 0u);
    ASSERT_EQ(loaded.indices.size(), 0u);
}
