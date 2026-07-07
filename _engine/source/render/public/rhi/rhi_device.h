#pragma once
#include "core/foundation_types.h"
#include "core/string/string_id.h"
#include "render/rhi/rhi_types.h"
#include "render/rhi/rhi_resources.h"
#include "render/rhi/rhi_pipeline.h"

namespace Entelechy
{

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
class IRHIDevice
{
public:
    virtual ~IRHIDevice() = default;

    // -- Resource creation ------------------------------------------------
    virtual RHIBufferRef createBuffer(const BufferDesc &desc, const void *initialData) = 0;
    virtual RHITextureRef createTexture(const TextureDesc &desc, const void *initialData) = 0;
    virtual RHIShaderRef createShader(ShaderStage stage, const void *bytecode, size_t size) = 0;
    virtual RHIPipelineStateRef createPipelineState(const PipelineStateDesc &desc) = 0;

    // -- Command context --------------------------------------------------
    virtual IRHICommandList *createCommandList() = 0;

    // -- Submission and presentation --------------------------------------
    virtual void submit(IRHICommandList *cmdList) = 0;
    virtual void present() = 0;

    // -- Frame fencing -----------------------------------------------------
    // Insert a GPU fence after all previously submitted commands and return
    // its monotonic value. Safe to call once per frame at the end of the
    // frame (after present).
    virtual RHIFenceValue signalFrame() = 0;

    // Return the highest frame value whose fence has been signaled by the GPU.
    // This is a non-blocking poll.
    virtual RHIFenceValue getCompletedFenceValue() = 0;

    // -- Resource lifecycle ------------------------------------------------
    // Queue a resource whose reference count reached zero for deferred
    // deletion. The device records the current frame fence and will not
    // destroy the resource until the GPU has finished that frame.
    virtual void queueResourceForDelete(GPUResource *resource) = 0;

    // Process the deferred-delete queue, freeing resources whose frame fence
    // has been signaled. Call once per frame on the rendering thread.
    virtual void flushPendingDeletes() = 0;

    // -- Memory budget -----------------------------------------------------
    // Best-effort query of GPU memory info. OpenGL backends use vendor
    // extensions (NVX/ATI); returns zeros if unsupported.
    virtual RHIMemoryInfo queryMemoryInfo() const = 0;

    // Memory usage tracked by the RHI (sum of memorySizeBytes() for all
    // resources currently considered resident). This always works, even when
    // queryMemoryInfo() returns zeros.
    virtual u64 getTrackedMemoryUsage() const = 0;

    // -- Queries ----------------------------------------------------------
    virtual RenderBackendType getBackendType() const = 0;

    // Capability queries (override per backend)
    virtual bool supportsComputeShaders() const
    {
        return false;
    }
    virtual bool supportsMultiThreadedRecording() const
    {
        return false;
    }
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
class IRHICommandList
{
public:
    virtual ~IRHICommandList() = default;

    // Lifecycle
    virtual void begin() = 0;
    virtual void end() = 0;

    // Render pass
    virtual void beginRenderPass(const RenderPassDesc &desc) = 0;
    virtual void endRenderPass() = 0;

    // Viewport and scissor
    virtual void setViewport(f32 x, f32 y, f32 w, f32 h) = 0;
    virtual void setScissor(u32 x, u32 y, u32 w, u32 h) = 0;

    // Pipeline and resource binding
    virtual void bindPipeline(RHIPipelineState *pso) = 0;
    virtual void bindVertexBuffer(RHIBuffer *buffer, u32 slot, u32 offset) = 0;
    virtual void bindIndexBuffer(RHIBuffer *buffer, u32 offset) = 0;

    // Draw commands
    virtual void drawIndexed(u32 indexCount, u32 startIndex, i32 baseVertex) = 0;
    virtual void draw(u32 vertexCount, u32 startVertex) = 0;

    // Resource barriers (simplified; no-op on GL backend)
    virtual void resourceBarrier(const BarrierDesc *barriers, u32 count) = 0;

    // Clear render target attachment (convenience)
    virtual void clearRenderTarget(u32 attachmentIndex, const f32 color[4]) = 0;

    // Phase 1 note: OpenGL-style immediate uniform setting.
    // Future backends (Vulkan/D3D12) implement these via push constants
    // or dynamic UBO offsets. This interface is intentionally minimal
    // and will be revisited when the bindless architecture is ready.
    virtual void setUniformFloat(StringId name, f32 value) = 0;
    virtual void setUniformInt(StringId name, i32 value) = 0;
    virtual void setUniformVec2(StringId name, const f32 *value) = 0;
    virtual void setUniformVec3(StringId name, const f32 *value) = 0;
    virtual void setUniformVec4(StringId name, const f32 *value) = 0;
    virtual void setUniformMat4(StringId name, const f32 *value, bool transpose = false) = 0;
    virtual void bindTexture(u32 slot, RHITexture *texture) = 0;

    // -- Debug markers -----------------------------------------------------
    // Map to platform debug groups (PIX events, RenderDoc labels, etc.)
    virtual void pushDebugGroup(const char *name) = 0;
    virtual void popDebugGroup() = 0;
    virtual void insertDebugMarker(const char *name) = 0;
};

} // namespace Entelechy
