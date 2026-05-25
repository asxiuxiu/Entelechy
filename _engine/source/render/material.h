#pragma once
#include "foundation_types.h"
#include "rhi_device.h"
#include "rhi_resources.h"
#include "rhi_pipeline.h"
#include "material_types.h"
#include "shader_cache.h"
#include "hash_map.h"
#include "small_string.h"
#include "math/vec.h"
#include "math/mat4.h"

namespace Entelechy {

// ------------------------------------------------------------------
// Material
//
// Phase 1 simplified material system:
// - Directly references a vertex + fragment shader pair (no template layer)
// - Synchronous compilation on init (no async build)
// - CPU-side uniform block with per-draw-call glUniform* upload
// - Single shared layout concept: parameters are set by name and uploaded
//   via IRHICommandList uniform-setting APIs
//
// Future extensions (Phase 2+):
// - ShaderTemplate reference for variant management
// - Async Technique compilation with fallback
// - GPU UBO + BindGroup pool
// ------------------------------------------------------------------
class Material {
public:
    Material();
    ~Material();

    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
    Material(Material&&) noexcept;
    Material& operator=(Material&&) noexcept;

    // Initialize with shader source strings and parameter layout.
    // If shaderCache is null, shaders are compiled without caching.
    bool init(IRHIDevice* device, ShaderCache* shaderCache,
              const char* vertexSource, const char* fragmentSource,
              const MaterialParamDesc* params, u32 paramCount,
              const PipelineStateDesc& pipelineDesc);

    void shutdown();

    // -- Parameter setters (CPU side, uploaded on bind) --------------------
    void setFloat(const char* name, f32 value);
    void setVec2(const char* name, const Vec2& value);
    void setVec3(const char* name, const Vec3& value);
    void setVec4(const char* name, const Vec4& value);
    void setMat4(const char* name, const Mat4& value, bool transpose = false);
    void setTexture(const char* name, RHITextureRef texture);

    // -- Rendering ---------------------------------------------------------
    // Bind PSO and upload all parameters to GPU.
    // Must be called within a render pass.
    void bind(IRHICommandList* cmdList);

    // -- Queries -----------------------------------------------------------
    bool isValid() const { return m_valid; }
    const PipelineStateDesc& getPipelineDesc() const { return m_pipelineDesc; }

private:
    struct ParamSlot {
        MaterialParamType type = MaterialParamType::Float;
        u32 offset = 0;      // bytes within m_uniformData (for uniform types)
        u32 textureSlot = 0; // texture unit index (for texture types)
    };

    bool m_valid = false;

    RHIShaderRef m_vertexShader;
    RHIShaderRef m_fragmentShader;
    PipelineStateDesc m_pipelineDesc;
    RHIPipelineStateRef m_pipelineState;

    u8* m_uniformData = nullptr;
    u32 m_uniformDataSize = 0;

    HashMap<SmallString, ParamSlot> m_params;
    HashMap<SmallString, RHITextureRef> m_textures;
};

} // namespace Entelechy
