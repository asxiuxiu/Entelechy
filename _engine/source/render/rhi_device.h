#pragma once
#include "foundation_types.h"
#include "rhi_types.h"
#include "rhi_resources.h"
#include "rhi_pipeline.h"

namespace Entelechy {

// Forward declarations
class IRHICommandList;

// ------------------------------------------------------------------
// IRHIDevice: GPU resource factory and queue manager
//
// Design aligns with UE's FDynamicRHI as the "maximum common
// denominator" across backends. Each backend implements resource
// creation in its own native API.
//
// Phase 1 note: OpenGL backend implements this with immediate
// execution. The interface shape is future-proof for D3D12/Vulkan.
// ------------------------------------------------------------------
class IRHIDevice {
public:
    virtual ~IRHIDevice() = default;

    // -- Resource creation ------------------------------------------------
    virtual RHIBufferRef createBuffer(const BufferDesc& desc, const void* initialData) = 0;
    virtual RHITextureRef createTexture(const TextureDesc& desc, const void* initialData) = 0;
    virtual RHIShaderRef createShader(ShaderStage stage, const void* bytecode, size_t size) = 0;
    virtual RHIPipelineStateRef createPipelineState(const PipelineStateDesc& desc) = 0;

    // -- Command context --------------------------------------------------
    virtual IRHICommandList* createCommandList() = 0;

    // -- Submission and presentation --------------------------------------
    virtual void submit(IRHICommandList* cmdList) = 0;
    virtual void present() = 0;

    // -- Queries ----------------------------------------------------------
    virtual RenderBackendType getBackendType() const = 0;

    // Capability queries (override per backend)
    virtual bool supportsComputeShaders() const { return false; }
    virtual bool supportsMultiThreadedRecording() const { return false; }
};

// ------------------------------------------------------------------
// IRHICommandList: Recording draw commands
//
// Design aligns with UE's FRHICommandList intent:
// - Upper layers only "issue commands" (Draw, SetViewport, BindPipeline)
// - Whether commands are buffered or executed immediately is an
//   implementation detail of the backend.
//
// Phase 1 note: OpenGL GLCommandList executes immediately. The
// interface is command-style to leave room for deferred execution.
// ------------------------------------------------------------------
class IRHICommandList {
public:
    virtual ~IRHICommandList() = default;

    // Lifecycle
    virtual void begin() = 0;
    virtual void end() = 0;

    // Render pass
    virtual void beginRenderPass(const RenderPassDesc& desc) = 0;
    virtual void endRenderPass() = 0;

    // Viewport and scissor
    virtual void setViewport(f32 x, f32 y, f32 w, f32 h) = 0;
    virtual void setScissor(u32 x, u32 y, u32 w, u32 h) = 0;

    // Pipeline and resource binding
    virtual void bindPipeline(RHIPipelineState* pso) = 0;
    virtual void bindVertexBuffer(RHIBuffer* buffer, u32 slot, u32 offset) = 0;
    virtual void bindIndexBuffer(RHIBuffer* buffer, u32 offset) = 0;

    // Draw commands
    virtual void drawIndexed(u32 indexCount, u32 startIndex, i32 baseVertex) = 0;
    virtual void draw(u32 vertexCount, u32 startVertex) = 0;

    // Resource barriers (simplified; no-op on GL backend)
    virtual void resourceBarrier(const BarrierDesc* barriers, u32 count) = 0;

    // Clear render target attachment (convenience)
    virtual void clearRenderTarget(u32 attachmentIndex, const f32 color[4]) = 0;

    // Phase 1 note: OpenGL-style immediate uniform setting.
    // Future backends (Vulkan/D3D12) implement these via push constants
    // or dynamic UBO offsets. This interface is intentionally minimal
    // and will be revisited when the bindless architecture is ready.
    virtual void setUniformFloat(const char* name, f32 value) = 0;
    virtual void setUniformInt(const char* name, i32 value) = 0;
    virtual void setUniformVec2(const char* name, const f32* value) = 0;
    virtual void setUniformVec3(const char* name, const f32* value) = 0;
    virtual void setUniformVec4(const char* name, const f32* value) = 0;
    virtual void setUniformMat4(const char* name, const f32* value, bool transpose = false) = 0;
    virtual void bindTexture(u32 slot, RHITexture* texture) = 0;
};

} // namespace Entelechy
