#include "test/test_framework.h"
#include "render/binding/shader_reflection.h"
#include <cstring>

using namespace Entelechy;

namespace
{

// Sample reflection JSON in the ShaderCompiler output format (pbr_lit pixel).
const char *kSampleReflection = R"({
  "name": "pbr_lit",
  "stage": "pixel",
  "cbuffers": [
    { "name": "PerFrame", "binding": 0, "size": 48, "members": [
        { "name": "uViewPos", "type": "vec4", "offset": 0, "size": 16 },
        { "name": "uLightDir", "type": "vec4", "offset": 16, "size": 16 },
        { "name": "uLightColor", "type": "vec4", "offset": 32, "size": 16 }
      ] },
    { "name": "PerMaterial", "binding": 1, "size": 32, "members": [
        { "name": "uColor", "type": "vec4", "offset": 0, "size": 16 },
        { "name": "uAlphaMR", "type": "vec4", "offset": 16, "size": 16 }
      ] }
  ],
  "textures": [
    { "name": "uBaseColorTex", "binding": 0 },
    { "name": "uNormalTex", "binding": 1 },
    { "name": "uMRTex", "binding": 2 }
  ]
})";

// A cbuffer whose declared size is not 16-aligned must be padded at load.
const char *kUnevenSizeReflection = R"({
  "cbuffers": [
    { "name": "Odd", "binding": 3, "size": 40, "members": [
        { "name": "a", "type": "vec4", "offset": 0, "size": 16 },
        { "name": "b", "type": "vec4", "offset": 16, "size": 16 }
      ] }
  ],
  "textures": []
})";

} // namespace

// ------------------------------------------------------------------
// Test: parse the ShaderCompiler reflection JSON
// ------------------------------------------------------------------
TEST(ShaderReflection, ParseSample)
{
    ShaderReflection reflection;
    ASSERT_TRUE(parseShaderReflection(kSampleReflection, std::strlen(kSampleReflection), reflection));

    ASSERT_EQ(reflection.name, String("pbr_lit"));
    ASSERT_EQ(static_cast<u32>(reflection.stage), static_cast<u32>(ShaderStage::Fragment));

    ASSERT_EQ(reflection.cbuffers.size(), 2u);

    // PerFrame: binding 0, 48 bytes, three vec4 members
    const ShaderReflectionCBuffer *perFrame = reflection.findCBufferByBinding(0);
    ASSERT_TRUE(perFrame != nullptr);
    ASSERT_EQ(perFrame->name, String("PerFrame"));
    ASSERT_EQ(perFrame->size, 48u);
    ASSERT_EQ(perFrame->members.size(), 3u);
    ASSERT_EQ(perFrame->members[0].name, String("uViewPos"));
    ASSERT_EQ(perFrame->members[0].type, ShaderMemberType::Vec4);
    ASSERT_EQ(perFrame->members[0].offset, 0u);
    ASSERT_EQ(perFrame->members[0].size, 16u);
    ASSERT_EQ(perFrame->members[2].name, String("uLightColor"));
    ASSERT_EQ(perFrame->members[2].offset, 32u);

    // PerMaterial: binding 1
    const ShaderReflectionCBuffer *perMaterial = reflection.findCBufferByBinding(1);
    ASSERT_TRUE(perMaterial != nullptr);
    ASSERT_EQ(perMaterial->name, String("PerMaterial"));
    ASSERT_EQ(perMaterial->members.size(), 2u);

    // findMember works
    const ShaderReflectionMember *alphaMR = perMaterial->findMember("uAlphaMR");
    ASSERT_TRUE(alphaMR != nullptr);
    ASSERT_EQ(alphaMR->offset, 16u);
    ASSERT_TRUE(perMaterial->findMember("does_not_exist") == nullptr);

    // Textures keep their t-registers
    ASSERT_EQ(reflection.textures.size(), 3u);
    ASSERT_EQ(reflection.textures[0].name, String("uBaseColorTex"));
    ASSERT_EQ(reflection.textures[0].binding, 0u);
    ASSERT_EQ(reflection.textures[1].binding, 1u);
    ASSERT_EQ(reflection.textures[2].binding, 2u);
}

// ------------------------------------------------------------------
// Test: cbuffer size is padded to 16 bytes
// ------------------------------------------------------------------
TEST(ShaderReflection, SizePaddedTo16)
{
    ShaderReflection reflection;
    ASSERT_TRUE(parseShaderReflection(kUnevenSizeReflection, std::strlen(kUnevenSizeReflection), reflection));

    ASSERT_EQ(reflection.cbuffers.size(), 1u);
    const ShaderReflectionCBuffer *odd = reflection.findCBufferByBinding(3);
    ASSERT_TRUE(odd != nullptr);
    // Declared 40 -> padded to 48 so ring allocations cover the std140 block.
    ASSERT_EQ(odd->size, 48u);
}

// ------------------------------------------------------------------
// Test: invalid JSON is rejected
// ------------------------------------------------------------------
TEST(ShaderReflection, RejectsGarbage)
{
    ShaderReflection reflection;
    ASSERT_FALSE(parseShaderReflection("not json at all", 15, reflection));
    ASSERT_FALSE(parseShaderReflection("[]", 2, reflection));
}
