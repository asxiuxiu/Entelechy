#pragma once
#include "core/foundation_types.h"
#include "core/string/string_id.h"
#include "render/rhi/rhi_types.h"
#include "render/rhi/rhi_resources.h"
#include "render/rhi/rhi_pipeline.h"
#include <vector>

namespace Entelechy
{

// Forward declarations
class IRHICommandList;
class IWindow;

// ------------------------------------------------------------------
// Render settings (clear color, vsync) — carried by the device
// ------------------------------------------------------------------
struct RenderSettings
{
    f32 clearColor[4] = {0.15f, 0.17f, 0.13f, 1.0f};
    bool vsync = true;
};

// ------------------------------------------------------------------
// IRHIDevice: Unified GPU device interface
//
// Combines resource factory, command submission, surface/window
// management, and frame synchronization into a single interface.
// Design follows UE's FDynamicRHI / SDL_GPUDevice pattern where
// the device owns both the GPU context and the presentation surface.
//
// Each backend (OpenGL, D3D12, Vulkan) implements this interface.
// ------------------------------------------------------------------
class IRHIDevice
{
public:
    virtual ~IRHIDevice() = default;

    // -- Surface lifecycle -------------------------------------------------
    // Initialize the rendering surface (window/context/swapchain).
    // Must be called before any other method.
    virtual bool initSurface(const SurfaceDesc &desc) = 0;

    // Tear down the surface. Safe to call multiple times.
    virtual void shutdownSurface() = 0;

    // Prepare for a new frame (e.g. acquire swapchain image, reset viewport).
    virtual void beginFrame() = 0;

    // End the current frame: present the rendered image and signal the
    // frame fence. Replaces the old separate present() + signalFrame().
    virtual void endFrame() = 0;

    // Set the clear color used by subsequent clear() calls.
    virtual void setClearColor(f32 r, f32 g, f32 b, f32 a) = 0;

    // Clear the specified attachments of the current framebuffer.
    virtual void clear(ClearFlags flags) = 0;

    // Handle window resize (recreate swapchain if needed).
    virtual void resizeSurface(u32 width, u32 height) = 0;

    // Debug/diagnostics: read the current back buffer as RGBA8 pixels with
    // top-left origin. Must be called after rendering, before endFrame()
    // presents. Backends without readback support return false.
    virtual bool readbackBackbuffer(std::vector<u8> &outPixelsRGBA8, u32 &outWidth, u32 &outHeight)
    {
        (void)outPixelsRGBA8;
        (void)outWidth;
        (void)outHeight;
        return false;
    }

    // -- Resource creation ------------------------------------------------
    virtual RHIBufferRef createBuffer(const BufferDesc &desc, const void *initialData) = 0;
    virtual RHITextureRef createTexture(const TextureDesc &desc, const void *initialData) = 0;

    // Create a shader from backend-specific bytecode.
    // GL: GLSL source text (format = GLSL). D3D12: DXIL binary. Vulkan: SPIR-V.
    virtual RHIShaderRef createShader(const ShaderBytecode &bytecode) = 0;

    virtual RHIPipelineStateRef createPipelineState(const PipelineStateDesc &desc) = 0;

    // Create a standalone fence object for multi-queue synchronization.
    virtual RHIFenceRef createFence(RHIFenceValue initialValue = 0) = 0;

    // -- Command context --------------------------------------------------
    virtual IRHICommandList *createCommandList() = 0;

    // -- Submission -------------------------------------------------------
    virtual void submit(IRHICommandList *cmdList) = 0;

    // -- Frame fencing -----------------------------------------------------
    // Insert a GPU fence after all previously submitted commands and return
    // its monotonic value. Called internally by endFrame(); exposed for
    // advanced use cases (e.g. async compute).
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
// OpenGL GLCommandList executes immediately. The interface is
// command-style to leave room for deferred execution.
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

    // Resource barriers: transition resources between states for correct
    // GPU access. No-op on OpenGL (driver manages state implicitly).
    // Required for D3D12/Vulkan correctness.
    virtual void resourceBarrier(const BarrierDesc *barriers, u32 count) = 0;

    // Clear render target attachment (convenience)
    virtual void clearRenderTarget(u32 attachmentIndex, const f32 color[4]) = 0;

    // OpenGL-style immediate uniform setting.
    // Future backends (Vulkan/D3D12) implement these via push constants
    // or dynamic UBO offsets. This interface is intentionally minimal
    // and will be revisited when the bindless architecture is ready.
    virtual void setUniformFloat(StringId name, f32 value) = 0;
    virtual void setUniformInt(StringId name, i32 value) = 0;
    virtual void setUniformVec2(StringId name, const f32 *value) = 0;
    virtual void setUniformVec3(StringId name, const f32 *value) = 0;
    virtual void setUniformVec4(StringId name, const f32 *value) = 0;
    virtual void setUniformMat3(StringId name, const f32 *value, bool transpose = false) = 0;
    virtual void setUniformMat4(StringId name, const f32 *value, bool transpose = false) = 0;
    virtual void bindTexture(u32 slot, RHITexture *texture) = 0;

    // -- Constant buffer / push constant binding ---------------------------
    // Bind a constant/uniform buffer at the given binding point.
    // D3D12: CBV descriptor table. Vulkan: descriptor set. GL: glBindBufferBase.
    virtual void bindConstantBuffer(u32 binding, RHIBuffer *buffer, u32 offset, u32 size) = 0;

    // Upload push constants inline (small per-draw data).
    // D3D12: root constants. Vulkan: vkCmdPushConstants. GL: no-op (uniforms used instead).
    virtual void setPushConstants(u32 offset, const void *data, u32 size) = 0;

    // -- Debug markers -----------------------------------------------------
    // Map to platform debug groups (PIX events, RenderDoc labels, etc.)
    virtual void pushDebugGroup(const char *name) = 0;
    virtual void popDebugGroup() = 0;
    virtual void insertDebugMarker(const char *name) = 0;
};

} // namespace Entelechy
