#include "test/test_framework.h"
#include "asset/loader/texture_asset_loader.h"
#include "asset/type/mesh_asset.h"
#include "asset/type/texture_asset.h"
#include <cstring>

// ------------------------------------------------------------------
// MeshAsset tests
// ------------------------------------------------------------------
TEST(Asset, MeshAssetComputeBounds)
{
    Entelechy::MeshAsset mesh;
    mesh.vertices.pushBack(Entelechy::MeshVertex{.position = {-1.0f, 0.0f, 2.0f}});
    mesh.vertices.pushBack(Entelechy::MeshVertex{.position = {3.0f, -2.0f, 0.0f}});
    mesh.vertices.pushBack(Entelechy::MeshVertex{.position = {0.0f, 5.0f, -4.0f}});
    mesh.computeBounds();

    ASSERT_EQ(mesh.bounds.min.x, -1.0f);
    ASSERT_EQ(mesh.bounds.min.y, -2.0f);
    ASSERT_EQ(mesh.bounds.min.z, -4.0f);
    ASSERT_EQ(mesh.bounds.max.x, 3.0f);
    ASSERT_EQ(mesh.bounds.max.y, 5.0f);
    ASSERT_EQ(mesh.bounds.max.z, 2.0f);
}

TEST(Asset, MeshAssetComputeBoundsEmpty)
{
    Entelechy::MeshAsset mesh;
    mesh.computeBounds();
    ASSERT_EQ(mesh.bounds.min.x, 0.0f);
    ASSERT_EQ(mesh.bounds.max.x, 0.0f);
}

TEST(Asset, MeshAssetVertexStride)
{
    // position(3) + normal(3) + uv(2) + tangent(3) + tangentW(1) = 12 floats
    ASSERT_EQ(Entelechy::MeshAsset::vertexStride(), 12u * sizeof(f32));
}

// ------------------------------------------------------------------
// TextureAsset / TextureAssetLoader tests
// ------------------------------------------------------------------
namespace
{

// 2x2 RGBA PNG, row-major top-left origin:
//   (0,0)=red (1,0)=green / (0,1)=blue (1,1)=white
// Generated offline with a minimal zlib/struct PNG writer.
const u8 kTestPng[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xB6, 0x0D, 0x24, 0x00, 0x00, 0x00,
    0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
    0x1F, 0x0C, 0x81, 0x34, 0x18, 0x00, 0x00, 0x49, 0xC8, 0x09, 0xF7, 0xF9,
    0xAB, 0xB6, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE,
    0x42, 0x60, 0x82,
};

Entelechy::FileData makeFileData(const u8 *bytes, usize count)
{
    Entelechy::FileData data;
    data.bytes.resize(count);
    std::memcpy(data.bytes.data(), bytes, count);
    data.valid = true;
    return data;
}

} // namespace

TEST(Asset, TextureLoaderDecodesPng)
{
    Entelechy::TextureAssetLoader loader;
    Entelechy::TextureAsset tex = loader.load(makeFileData(kTestPng, sizeof(kTestPng)), Entelechy::Path{"test.png"});

    ASSERT_TRUE(tex.valid());
    ASSERT_EQ(tex.width, 2u);
    ASSERT_EQ(tex.height, 2u);
    ASSERT_EQ(tex.pixels.size(), 2u * 2u * 4u);

    // (0,0) red
    ASSERT_EQ(tex.pixels[0], 255);
    ASSERT_EQ(tex.pixels[1], 0);
    ASSERT_EQ(tex.pixels[2], 0);
    ASSERT_EQ(tex.pixels[3], 255);
    // (1,0) green
    ASSERT_EQ(tex.pixels[4], 0);
    ASSERT_EQ(tex.pixels[5], 255);
    // (0,1) blue
    ASSERT_EQ(tex.pixels[10], 255);
    // (1,1) white
    ASSERT_EQ(tex.pixels[12], 255);
    ASSERT_EQ(tex.pixels[13], 255);
    ASSERT_EQ(tex.pixels[14], 255);
}

TEST(Asset, TextureLoaderRejectsGarbage)
{
    const u8 garbage[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    Entelechy::TextureAssetLoader loader;
    Entelechy::TextureAsset tex = loader.load(makeFileData(garbage, sizeof(garbage)), Entelechy::Path{"bad.png"});
    ASSERT_FALSE(tex.valid());
}

TEST(Asset, TextureLoaderRejectsInvalidFileData)
{
    Entelechy::TextureAssetLoader loader;
    Entelechy::TextureAsset tex = loader.load(Entelechy::FileData{}, Entelechy::Path{"missing.png"});
    ASSERT_FALSE(tex.valid());
}
