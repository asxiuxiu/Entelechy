#pragma once
#include "core/foundation_types.h"
#include "render/rhi/rhi_device.h"
#include "render/rhi/rhi_resources.h"
#include "render/rhi/rhi_pipeline.h"
#include "render/material/material_types.h"
#include "render/binding/bind_group.h"
#include "render/binding/shader_reflection.h"
#include "core/container/dynamic_array.h"
#include "core/container/hash_map.h"
#include "core/string/string_id.h"
#include "core/string/string_intern_pool.h"
#include "core/math/vec.h"
#include "core/math/mat4.h"
#include "core/math/mat3.h"

namespace Entelechy
{

class ConstantBufferRing;

// ------------------------------------------------------------------
// Material
//
// Simplified material system (reflection-driven, 6e):
// - Directly references a vertex + fragment shader pair (no template layer)
// - cbuffer layout comes from the offline reflection JSON (ShaderReflection),
//   so parameter names are the real HLSL member names ("uViewPos", "uMVP")
//   — no SPIRV-Cross flattened `type_*[N]` names anywhere
// - CPU-side constant blob laid out per the reflection; bind() memcpys each
//   cbuffer into a ConstantBufferRing block and binds a BindGroup
//   (bindConstantBuffer / bindTexture), replacing per-param glUniform*
// - cbuffer members are authored vec4/mat4-only so the single blob layout is
//   identical under HLSL packing (D3D12) and std140 (GL UBO)
// ------------------------------------------------------------------
class Material
{
public:
    Material();
    ~Material();

    Material(const Material &) = delete;
    Material &operator=(const Material &) = delete;
    Material(Material &&) noexcept;
    Material &operator=(Material &&) noexcept;

    // Initialize from precompiled bytecode files (DXIL/SPIR-V/GLSL) plus the
    // matching `_reflection.json` metadata files (same base name). Reads the
    // bytecode and reflection from disk and creates shaders directly.
    bool initFromBytecode(IRHIDevice *device,
                          const char *vertexBytecodePath, const char *fragmentBytecodePath,
                          ShaderBytecodeFormat format,
                          const MaterialParamDesc *params, u32 paramCount,
                          const PipelineStateDesc &pipelineDesc);

    void shutdown();

    // -- Parameter setters (CPU side, uploaded on bind) --------------------
    // `name` is the real cbuffer member name from the shader reflection.
    void setFloat(StringId name, f32 value);
    void setVec2(StringId name, const Vec2 &value);
    void setVec3(StringId name, const Vec3 &value);
    void setVec4(StringId name, const Vec4 &value);
    void setMat3(StringId name, const Mat3 &value);
    void setMat4(StringId name, const Mat4 &value);
    void setTexture(StringId name, RHITextureRef texture);

    // -- Rendering ---------------------------------------------------------
    // Binds the PSO, uploads every cbuffer into the ring and binds the
    // BindGroup (constant buffers by binding point, textures by t-register).
    // Must be called within a render pass.
    void bind(IRHICommandList *cmdList, ConstantBufferRing *ring);

    // -- Queries -----------------------------------------------------------
    bool isValid() const
    {
        return m_valid;
    }
    const PipelineStateDesc &getPipelineDesc() const
    {
        return m_pipeline_desc;
    }
    const BindGroupLayout &bindLayout() const
    {
        return m_bind_layout;
    }

private:
    struct CBufferSlot
    {
        u32 binding = 0;
        u32 blobOffset = 0; // offset of this cbuffer within m_uniform_data
        u32 size = 0;       // padded block size
        ShaderStage stage = ShaderStage::None;
    };

    struct ParamSlot
    {
        u32 cbufferIndex = 0;    // index into m_cbuffers
        u32 memberOffset = 0;    // bytes within the cbuffer
        ShaderMemberType memberType = ShaderMemberType::Unknown;
        u32 textureSlot = 0;     // t-register for texture params
        bool isTexture = false;
    };

    // Merge a reflection cbuffer into the binding-sorted list.
    void insertCBuffer(const ShaderReflectionCBuffer &ref, ShaderStage stage);

    // Build the param table from the merged VS+PS reflections.
    bool buildParamLayout(const MaterialParamDesc *params, u32 paramCount,
                          const ShaderReflection &vsReflection, const ShaderReflection &psReflection);

    bool m_valid = false;

    RHIShaderRef m_vertex_shader;
    RHIShaderRef m_fragment_shader;
    PipelineStateDesc m_pipeline_desc;
    RHIPipelineStateRef m_pipeline_state;

    // Merged cbuffer list (VS + PS), sorted by binding. Unique bindings
    // across stages per material (GL shares one UBO binding namespace).
    DynamicArray<CBufferSlot> m_cbuffers;
    BindGroupLayout m_bind_layout;
    BindGroup m_bind_group;

    u8 *m_uniform_data = nullptr;
    u32 m_uniform_data_size = 0;

    HashMap<StringId, ParamSlot> m_params;
    HashMap<StringId, RHITextureRef> m_textures;
};

} // namespace Entelechy
