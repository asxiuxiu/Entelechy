#pragma once
#include "render/rhi/rhi_device.h"
#include "render/rhi/rhi_pipeline.h"
#include "core/container/dynamic_array.h"
#include <glad/glad.h>
#include <memory>

namespace Entelechy
{

// Forward declarations for deferred command buffer types.
// Full definitions are in render_command_buffer.h, deferred_command_list.h,
// gl_command_translator.h — included only in the .cpp file.
class RenderCommandBuffer;
class DeferredCommandList;
class GLCommandTranslator;

class IWindow;

// ==================================================================
// OpenGL resource implementations
// ==================================================================

class GLBuffer : public RHIBuffer
{
public:
    GLBuffer(u32 size, BufferUsage usage, GLuint vbo, GLuint vao, void *mapped);
    ~GLBuffer() override;

    u32 getSize() const override
    {
        return m_size;
    }
    BufferUsage getUsage() const override
    {
        return m_usage;
    }

    GLuint getVBO() const
    {
        return m_vbo;
    }
    GLuint getVAO() const
    {
        return m_vao;
    }

    void *getCpuMappedPointer() override
    {
        return m_mapped;
    }

    u64 memorySizeBytes() const override
    {
        return static_cast<u64>(m_size);
    }
    void setDebugName(const String &name) override;

protected:
    void onDestroy() override;

private:
    u32 m_size = 0;
    BufferUsage m_usage = BufferUsage::None;
    GLuint m_vbo = 0;
    GLuint m_vao = 0; // 0 if not a vertex buffer with layout
    void *m_mapped = nullptr; // persistent CPU map (cpuAccessible buffers)
};

class GLTexture : public RHITexture
{
public:
    GLTexture(const TextureDesc &desc, GLuint texture, GLenum target);
    ~GLTexture() override;

    const TextureDesc &getDesc() const override
    {
        return m_desc;
    }
    GLuint getTexture() const
    {
        return m_texture;
    }
    GLenum getTarget() const
    {
        return m_target;
    }

    u64 memorySizeBytes() const override;
    void setDebugName(const String &name) override;

protected:
    void onDestroy() override;

private:
    TextureDesc m_desc;
    GLuint m_texture = 0;
    GLenum m_target = GL_TEXTURE_2D;
};

class GLShader : public RHIShader
{
public:
    GLShader(ShaderStage stage, GLuint shader);
    ~GLShader() override;

    ShaderStage getStage() const override
    {
        return m_stage;
    }
    GLuint getShader() const
    {
        return m_shader;
    }

    u64 memorySizeBytes() const override
    {
        return 0;
    } // Driver-managed, unknown
    void setDebugName(const String &name) override;

protected:
    void onDestroy() override;

private:
    ShaderStage m_stage = ShaderStage::None;
    GLuint m_shader = 0;
};

class GLPipelineState : public RHIPipelineState
{
public:
    GLPipelineState(const PipelineStateDesc &desc, GLuint program);
    ~GLPipelineState() override;

    GLuint getProgram() const
    {
        return m_program;
    }
    const PipelineStateDesc &getDesc() const
    {
        return m_desc;
    }

    u64 memorySizeBytes() const override
    {
        return 0;
    } // Driver-managed, unknown
    void setDebugName(const String &name) override;

protected:
    void onDestroy() override;

private:
    PipelineStateDesc m_desc;
    GLuint m_program = 0;
};

// ==================================================================
// OpenGL Fence implementation
// ==================================================================

class GLFence : public IRHIFence
{
public:
    explicit GLFence(RHIFenceValue initialValue = 0);
    ~GLFence() override;

    void signal(RHIFenceValue value) override;
    bool wait(RHIFenceValue value, u64 timeoutNs = UINT64_MAX) override;
    RHIFenceValue getCompletedValue() const override;
    bool isSignaled(RHIFenceValue value) const override;

protected:
    void onDestroy() override;

private:
    mutable GLsync m_sync = nullptr;
    RHIFenceValue m_signaled_value = 0;
};

// ==================================================================
// OpenGL RHI Device
// ==================================================================

class GLRHIDevice : public IRHIDevice
{
public:
    explicit GLRHIDevice(IWindow *window = nullptr);
    ~GLRHIDevice() override;

    // Legacy init (no surface). Prefer initSurface() for new code.
    bool initialize();
    void shutdown();

    // -- Surface lifecycle (IRHIDevice) ------------------------------------
    bool initSurface(const SurfaceDesc &desc) override;
    void shutdownSurface() override;
    void beginFrame() override;
    void endFrame() override;
    void setClearColor(f32 r, f32 g, f32 b, f32 a) override;
    void clear(ClearFlags flags) override;
    void resizeSurface(u32 width, u32 height) override;
    bool readbackBackbuffer(std::vector<u8> &outPixelsRGBA8, u32 &outWidth, u32 &outHeight) override;

    // -- Resource creation (IRHIDevice) ------------------------------------
    RHIBufferRef createBuffer(const BufferDesc &desc, const void *initialData) override;
    RHITextureRef createTexture(const TextureDesc &desc, const void *initialData) override;
    RHIShaderRef createShader(const ShaderBytecode &bytecode) override;
    RHIPipelineStateRef createPipelineState(const PipelineStateDesc &desc) override;
    RHIFenceRef createFence(RHIFenceValue initialValue = 0) override;

    IRHICommandList *createCommandList() override;

    void submit(IRHICommandList *cmdList) override;

    RHIFenceValue signalFrame() override;
    RHIFenceValue getCompletedFenceValue() override;

    void queueResourceForDelete(GPUResource *resource) override;
    void flushPendingDeletes() override;

    RHIMemoryInfo queryMemoryInfo() const override;
    u64 getTrackedMemoryUsage() const override
    {
        return m_tracked_memory_bytes;
    }

    RenderBackendType getBackendType() const override
    {
        return RenderBackendType::OpenGL;
    }

    PSOManager &getPSOManager()
    {
        return m_pso_manager;
    }

    // Utility: estimate GPU memory for a texture description. Public so tests
    // and tooling can use the same calculation as the device.
    static u64 textureMemorySizeBytes(const TextureDesc &desc);

private:
    struct PendingDelete
    {
        GPUResource *resource = nullptr;
        RHIFenceValue fence = 0;
    };

    struct FrameFence
    {
        RHIFenceValue frame = 0;
        GLsync sync = nullptr;
    };

    void trackResourceCreated(const GPUResource *resource);
    void trackResourceDestroyed(const GPUResource *resource);

    // Raw PSO creation (no cache). The public createPipelineState routes
    // through m_pso_manager; this is the uncached worker.
    RHIPipelineStateRef createPipelineStateUncached(const PipelineStateDesc &desc);

    IWindow *m_window = nullptr;
    RenderSettings m_settings;
    PSOManager m_pso_manager;

    // Deferred command buffer: records commands for later replay by the translator.
    // Owned via unique_ptr because RenderCommandBuffer/DeferredCommandList/GLCommandTranslator
    // are forward-declared in this header (full types only visible in .cpp).
    struct DeferredState;
    std::unique_ptr<DeferredState> m_deferred;

    bool m_initialized = false;

    DynamicArray<PendingDelete> m_pending_deletes;
    DynamicArray<FrameFence> m_frame_fences;
    RHIFenceValue m_next_frame_value = 1;
    RHIFenceValue m_completed_frame_value = 0;

    u64 m_tracked_memory_bytes = 0;
};

} // namespace Entelechy
