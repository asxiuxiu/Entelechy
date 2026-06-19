#pragma once
#include "render/rhi/rhi_device.h"
#include "render/rhi/rhi_pipeline.h"
#include "core/container/dynamic_array.h"
#include "core/container/hash_map.h"
#include "core/string/string_id.h"
#include <glad/glad.h>

namespace Entelechy {

// ==================================================================
// OpenGL resource implementations
// ==================================================================

class GLBuffer : public RHIBuffer {
public:
    GLBuffer(u32 size, BufferUsage usage, GLuint vbo, GLuint vao);
    ~GLBuffer() override;

    u32 getSize() const override { return m_size; }
    BufferUsage getUsage() const override { return m_usage; }

    GLuint getVBO() const { return m_vbo; }
    GLuint getVAO() const { return m_vao; }

    u64 memorySizeBytes() const override { return static_cast<u64>(m_size); }
    void setDebugName(const String& name) override;

protected:
    void onDestroy() override;

private:
    u32 m_size = 0;
    BufferUsage m_usage = BufferUsage::None;
    GLuint m_vbo = 0;
    GLuint m_vao = 0; // 0 if not a vertex buffer with layout
};

class GLTexture : public RHITexture {
public:
    GLTexture(const TextureDesc& desc, GLuint texture, GLenum target);
    ~GLTexture() override;

    const TextureDesc& getDesc() const override { return m_desc; }
    GLuint getTexture() const { return m_texture; }
    GLenum getTarget() const { return m_target; }

    u64 memorySizeBytes() const override;
    void setDebugName(const String& name) override;

protected:
    void onDestroy() override;

private:
    TextureDesc m_desc;
    GLuint m_texture = 0;
    GLenum m_target = GL_TEXTURE_2D;
};

class GLShader : public RHIShader {
public:
    GLShader(ShaderStage stage, GLuint shader);
    ~GLShader() override;

    ShaderStage getStage() const override { return m_stage; }
    GLuint getShader() const { return m_shader; }

    u64 memorySizeBytes() const override { return 0; } // Driver-managed, unknown
    void setDebugName(const String& name) override;

protected:
    void onDestroy() override;

private:
    ShaderStage m_stage = ShaderStage::None;
    GLuint m_shader = 0;
};

class GLPipelineState : public RHIPipelineState {
public:
    GLPipelineState(const PipelineStateDesc& desc, GLuint program);
    ~GLPipelineState() override;

    GLuint getProgram() const { return m_program; }
    const PipelineStateDesc& getDesc() const { return m_desc; }

    u64 memorySizeBytes() const override { return 0; } // Driver-managed, unknown
    void setDebugName(const String& name) override;

protected:
    void onDestroy() override;

private:
    PipelineStateDesc m_desc;
    GLuint m_program = 0;
};

// ==================================================================
// Uniform location cache key
// ==================================================================
struct UniformLocKey {
    GLuint program = 0;
    StringId name;

    bool operator==(const UniformLocKey& other) const {
        return program == other.program && name == other.name;
    }
};

struct UniformLocKeyHash {
    u64 operator()(const UniformLocKey& key) const {
        u64 h = static_cast<u64>(key.program);
        h ^= key.name.value() + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

// ==================================================================
// OpenGL Command List (immediate execution for Phase 1)
// ==================================================================

class GLCommandList : public IRHICommandList {
public:
    GLCommandList() = default;

    void begin() override;
    void end() override;

    void beginRenderPass(const RenderPassDesc& desc) override;
    void endRenderPass() override;

    void setViewport(f32 x, f32 y, f32 w, f32 h) override;
    void setScissor(u32 x, u32 y, u32 w, u32 h) override;

    void bindPipeline(RHIPipelineState* pso) override;
    void bindVertexBuffer(RHIBuffer* buffer, u32 slot, u32 offset) override;
    void bindIndexBuffer(RHIBuffer* buffer, u32 offset) override;

    void drawIndexed(u32 indexCount, u32 startIndex, i32 baseVertex) override;
    void draw(u32 vertexCount, u32 startVertex) override;

    void resourceBarrier(const BarrierDesc* barriers, u32 count) override;
    void clearRenderTarget(u32 attachmentIndex, const f32 color[4]) override;

    // Uniform and texture binding (Phase 1 immediate GL)
    void setUniformFloat(StringId name, f32 value) override;
    void setUniformInt(StringId name, i32 value) override;
    void setUniformVec2(StringId name, const f32* value) override;
    void setUniformVec3(StringId name, const f32* value) override;
    void setUniformVec4(StringId name, const f32* value) override;
    void setUniformMat4(StringId name, const f32* value, bool transpose = false) override;
    void bindTexture(u32 slot, RHITexture* texture) override;

    // Debug markers
    void pushDebugGroup(const char* name) override;
    void popDebugGroup() override;
    void insertDebugMarker(const char* name) override;

private:
    // Cached uniform location lookup (program + name) -> location.
    // Eliminates per-draw-call string hashing inside the GL driver.
    GLint getUniformLocation(StringId name);

    bool m_inside_render_pass = false;
    GLuint m_bound_program = 0;
    GLuint m_bound_vao = 0;
    GLuint m_bound_ebo = 0;
    u32 m_ebo_offset = 0;

    HashMap<UniformLocKey, GLint, UniformLocKeyHash> m_uniform_cache;
    u32 m_debug_group_depth = 0;
};

// ==================================================================
// OpenGL RHI Device
// ==================================================================

class GLRHIDevice : public IRHIDevice {
public:
    GLRHIDevice();
    ~GLRHIDevice() override;

    bool initialize();
    void shutdown();

    // IRHIDevice interface
    RHIBufferRef createBuffer(const BufferDesc& desc, const void* initialData) override;
    RHITextureRef createTexture(const TextureDesc& desc, const void* initialData) override;
    RHIShaderRef createShader(ShaderStage stage, const void* bytecode, size_t size) override;
    RHIPipelineStateRef createPipelineState(const PipelineStateDesc& desc) override;

    IRHICommandList* createCommandList() override;

    void submit(IRHICommandList* cmdList) override;
    void present() override;

    RHIFenceValue signalFrame() override;
    RHIFenceValue getCompletedFenceValue() override;

    void queueResourceForDelete(GPUResource* resource) override;
    void flushPendingDeletes() override;

    RHIMemoryInfo queryMemoryInfo() const override;
    u64 getTrackedMemoryUsage() const override { return m_tracked_memory_bytes; }

    RenderBackendType getBackendType() const override { return RenderBackendType::OpenGL; }

    PSOManager& getPSOManager() { return m_pso_manager; }

    // Utility: estimate GPU memory for a texture description. Public so tests
    // and tooling can use the same calculation as the device.
    static u64 textureMemorySizeBytes(const TextureDesc& desc);

private:
    struct PendingDelete {
        GPUResource* resource = nullptr;
        RHIFenceValue fence = 0;
    };

    struct FrameFence {
        RHIFenceValue frame = 0;
        GLsync sync = nullptr;
    };

    void trackResourceCreated(const GPUResource* resource);
    void trackResourceDestroyed(const GPUResource* resource);

    PSOManager m_pso_manager;
    GLCommandList m_cmd_list;
    bool m_initialized = false;

    DynamicArray<PendingDelete> m_pending_deletes;
    DynamicArray<FrameFence> m_frame_fences;
    RHIFenceValue m_next_frame_value = 1;
    RHIFenceValue m_completed_frame_value = 0;

    u64 m_tracked_memory_bytes = 0;
};

} // namespace Entelechy
