#include "test/test_framework.h"
#include "asset/loader/material_asset_loader.h"
#include <cstring>

// ------------------------------------------------------------------
// MaterialAssetLoader / .emat format tests
// ------------------------------------------------------------------
namespace
{

Entelechy::FileData makeFileData(const char *json)
{
    Entelechy::FileData data;
    const usize len = std::strlen(json);
    data.bytes.resize(len);
    if (len > 0)
        std::memcpy(data.bytes.data(), json, len);
    data.valid = true;
    return data;
}

Entelechy::FileData makeFileDataTruncated(const char *json, usize dropBytes)
{
    Entelechy::FileData data;
    const usize len = std::strlen(json) - dropBytes;
    data.bytes.resize(len);
    std::memcpy(data.bytes.data(), json, len);
    data.valid = true;
    return data;
}

constexpr const char *kFullEmat =
    "{\"base_color_texture\":\"sponza/textures/brick_BaseColor.png\","
    "\"normal_texture\":\"sponza/textures/brick_Normal.png\","
    "\"mr_texture\":\"sponza/textures/brick_RM.png\","
    "\"base_color_factor\":[0.5,0.25,0.75],"
    "\"metallic_factor\":0.125,\"roughness_factor\":0.625,"
    "\"alpha_mode\":\"mask\",\"alpha_cutoff\":0.35,\"double_sided\":true}\n";

// A rejected file comes back as the cooker-schema default material.
void assertDefaultMaterial(const Entelechy::MaterialAsset &material)
{
    ASSERT_EQ(material.base_color.x, 1.0f);
    ASSERT_EQ(material.base_color.y, 1.0f);
    ASSERT_EQ(material.base_color.z, 1.0f);
    ASSERT_EQ(material.metallic_factor, 1.0f);
    ASSERT_EQ(material.roughness_factor, 1.0f);
    ASSERT_EQ(material.alpha_cutoff, 0.5f);
    ASSERT_TRUE(material.alpha_mode == Entelechy::AlphaMode::Opaque);
    ASSERT_FALSE(material.double_sided);
    ASSERT_TRUE(material.base_color_texture_path.empty());
    ASSERT_TRUE(material.normal_texture_path.empty());
    ASSERT_TRUE(material.mr_texture_path.empty());
    ASSERT_FALSE(material.base_color_texture.valid());
    ASSERT_FALSE(material.normal_texture.valid());
    ASSERT_FALSE(material.mr_texture.valid());
}

} // namespace

TEST(Asset, MaterialLoaderRoundTrip)
{
    Entelechy::MaterialAssetLoader loader;
    const Entelechy::MaterialAsset loaded = loader.load(makeFileData(kFullEmat), Entelechy::Path{"test.emat"});

    ASSERT_TRUE(loaded.base_color_texture_path == "sponza/textures/brick_BaseColor.png");
    ASSERT_TRUE(loaded.normal_texture_path == "sponza/textures/brick_Normal.png");
    ASSERT_TRUE(loaded.mr_texture_path == "sponza/textures/brick_RM.png");
    ASSERT_EQ(loaded.base_color.x, 0.5f);
    ASSERT_EQ(loaded.base_color.y, 0.25f);
    ASSERT_EQ(loaded.base_color.z, 0.75f);
    ASSERT_EQ(loaded.metallic_factor, 0.125f);
    ASSERT_EQ(loaded.roughness_factor, 0.625f);
    ASSERT_TRUE(loaded.alpha_mode == Entelechy::AlphaMode::Mask);
    ASSERT_EQ(loaded.alpha_cutoff, 0.35f);
    ASSERT_TRUE(loaded.double_sided);
    // The loader never resolves texture handles (handles are back-filled
    // on the spawn side).
    ASSERT_FALSE(loaded.base_color_texture.valid());
    ASSERT_FALSE(loaded.normal_texture.valid());
    ASSERT_FALSE(loaded.mr_texture.valid());
}

TEST(Asset, MaterialLoaderMissingFieldsUseDefaults)
{
    Entelechy::MaterialAssetLoader loader;
    const Entelechy::MaterialAsset loaded =
        loader.load(makeFileData("{\"base_color_texture\":\"sponza/textures/a.png\"}"), Entelechy::Path{"min.emat"});

    ASSERT_TRUE(loaded.base_color_texture_path == "sponza/textures/a.png");
    ASSERT_EQ(loaded.base_color.x, 1.0f);
    ASSERT_EQ(loaded.metallic_factor, 1.0f);
    ASSERT_EQ(loaded.roughness_factor, 1.0f);
    ASSERT_TRUE(loaded.alpha_mode == Entelechy::AlphaMode::Opaque);
    ASSERT_EQ(loaded.alpha_cutoff, 0.5f);
    ASSERT_FALSE(loaded.double_sided);
    ASSERT_TRUE(loaded.normal_texture_path.empty());
    ASSERT_TRUE(loaded.mr_texture_path.empty());
}

TEST(Asset, MaterialLoaderEmptyObjectUsesDefaults)
{
    Entelechy::MaterialAssetLoader loader;
    const Entelechy::MaterialAsset loaded = loader.load(makeFileData("{}"), Entelechy::Path{"empty.emat"});
    assertDefaultMaterial(loaded);
}

TEST(Asset, MaterialLoaderAlphaModes)
{
    Entelechy::MaterialAssetLoader loader;
    const Entelechy::MaterialAsset opaque =
        loader.load(makeFileData("{\"alpha_mode\":\"opaque\"}"), Entelechy::Path{"opaque.emat"});
    ASSERT_TRUE(opaque.alpha_mode == Entelechy::AlphaMode::Opaque);
    const Entelechy::MaterialAsset blend =
        loader.load(makeFileData("{\"alpha_mode\":\"blend\"}"), Entelechy::Path{"blend.emat"});
    ASSERT_TRUE(blend.alpha_mode == Entelechy::AlphaMode::Blend);
}

TEST(Asset, MaterialLoaderRejectsGarbage)
{
    Entelechy::FileData garbage;
    garbage.bytes.resize(64);
    for (usize i = 0; i < garbage.bytes.size(); ++i)
        garbage.bytes[i] = static_cast<u8>(i * 37u + 1u);
    garbage.valid = true;

    Entelechy::MaterialAssetLoader loader;
    assertDefaultMaterial(loader.load(garbage, Entelechy::Path{"bad.emat"}));
}

TEST(Asset, MaterialLoaderRejectsTruncated)
{
    Entelechy::MaterialAssetLoader loader;
    // Cut the file mid double_sided value; none of the non-default
    // fields may leak through.
    assertDefaultMaterial(loader.load(makeFileDataTruncated(kFullEmat, 3), Entelechy::Path{"truncated.emat"}));
}

TEST(Asset, MaterialLoaderRejectsWrongValueType)
{
    Entelechy::MaterialAssetLoader loader;
    assertDefaultMaterial(
        loader.load(makeFileData("{\"metallic_factor\":\"shiny\"}"), Entelechy::Path{"type.emat"}));
}

TEST(Asset, MaterialLoaderRejectsUnknownAlphaMode)
{
    Entelechy::MaterialAssetLoader loader;
    assertDefaultMaterial(
        loader.load(makeFileData("{\"alpha_mode\":\"additive\"}"), Entelechy::Path{"alpha.emat"}));
}

TEST(Asset, MaterialLoaderRejectsUnknownKey)
{
    Entelechy::MaterialAssetLoader loader;
    assertDefaultMaterial(loader.load(makeFileData("{\"emissive_factor\":[0,0,0]}"), Entelechy::Path{"key.emat"}));
}

TEST(Asset, MaterialLoaderRejectsInvalidFileData)
{
    Entelechy::MaterialAssetLoader loader;
    assertDefaultMaterial(loader.load(Entelechy::FileData{}, Entelechy::Path{"missing.emat"}));
}
