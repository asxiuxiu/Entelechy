#pragma once
#include "core/foundation_types.h"
#include "core/string/string.h"
#include "core/container/dynamic_array.h"
#include "render/rhi/rhi_types.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// ShaderReflection — cbuffer / texture binding layout loaded from the
// offline reflection JSON emitted by the ShaderCompiler tool (6e).
//
// Drives the UBO/CBV unified binding path: the CPU constant blob is laid
// out from the reflected member offsets (one shared layout for GL std140
// UBOs and D3D12 constant buffers — the shader cbuffers are authored
// vec4/mat4-only so both packing rules agree), and textures bind by their
// reflected t-register instead of any SPIRV-Cross generated name.
// ------------------------------------------------------------------

enum class ShaderMemberType : u8
{
    Float,
    Vec2,
    Vec3,
    Vec4,
    Mat3,
    Mat4,
    Int,
    Uint,
    Bool,
    Unknown,
};

struct ShaderReflectionMember
{
    String name;
    ShaderMemberType type = ShaderMemberType::Unknown;
    u32 offset = 0; // bytes within the cbuffer (std140 / HLSL packing)
    u32 size = 0;   // padded byte size of the member
};

struct ShaderReflectionCBuffer
{
    String name;
    u32 binding = 0; // b-register / GL_UNIFORM_BUFFER binding point
    u32 size = 0;    // block size padded to 16 bytes
    DynamicArray<ShaderReflectionMember> members;

    const ShaderReflectionMember *findMember(const String &name) const;
};

struct ShaderReflectionTexture
{
    String name;
    u32 binding = 0; // t-register / texture unit
};

struct ShaderReflection
{
    String name;
    ShaderStage stage = ShaderStage::None;
    DynamicArray<ShaderReflectionCBuffer> cbuffers;
    DynamicArray<ShaderReflectionTexture> textures;

    const ShaderReflectionCBuffer *findCBufferByBinding(u32 binding) const;
};

// Parses the reflection JSON text (null-terminated) produced by the
// ShaderCompiler tool. cbuffer sizes are rounded up to 16 bytes so ring
// allocations always cover the full std140 block. Returns false on parse
// failure; `out` is left in a partial but valid state.
bool parseShaderReflection(const char *text, usize length, ShaderReflection &out);

} // namespace Entelechy
