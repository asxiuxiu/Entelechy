#pragma once
#include "render/rhi/rhi_device.h"
#include "render/rhi/render_command_buffer.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// DeferredCommandList — records IRHICommandList calls into a
// RenderCommandBuffer instead of executing them immediately.
//
// This is the backend-agnostic recording layer. Each backend provides
// a translator (e.g. GLCommandTranslator) that replays the buffer.
// ------------------------------------------------------------------
class DeferredCommandList : public IRHICommandList
{
public:
    explicit DeferredCommandList(RenderCommandBuffer &buffer);

    // Lifecycle
    void begin() override;
    void end() override;

    // Render pass
    void beginRenderPass(const RenderPassDesc &desc) override;
    void endRenderPass() override;

    // Viewport and scissor
    void setViewport(f32 x, f32 y, f32 w, f32 h) override;
    void setScissor(u32 x, u32 y, u32 w, u32 h) override;

    // Pipeline and resource binding
    void bindPipeline(RHIPipelineState *pso) override;
    void bindVertexBuffer(RHIBuffer *buffer, u32 slot, u32 offset) override;
    void bindIndexBuffer(RHIBuffer *buffer, u32 offset) override;

    // Draw commands
    void drawIndexed(u32 indexCount, u32 startIndex, i32 baseVertex) override;
    void draw(u32 vertexCount, u32 startVertex) override;

    // Resource barriers
    void resourceBarrier(const BarrierDesc *barriers, u32 count) override;

    // Clear
    void clearRenderTarget(u32 attachmentIndex, const f32 color[4]) override;

    // Uniform setters
    void setUniformFloat(StringId name, f32 value) override;
    void setUniformInt(StringId name, i32 value) override;
    void setUniformVec2(StringId name, const f32 *value) override;
    void setUniformVec3(StringId name, const f32 *value) override;
    void setUniformVec4(StringId name, const f32 *value) override;
    void setUniformMat3(StringId name, const f32 *value, bool transpose = false) override;
    void setUniformMat4(StringId name, const f32 *value, bool transpose = false) override;
    void bindTexture(u32 slot, RHITexture *texture) override;

    // Constant buffer / push constants
    void bindConstantBuffer(u32 binding, RHIBuffer *buffer, u32 offset, u32 size) override;
    void setPushConstants(u32 offset, const void *data, u32 size) override;

    // Debug markers
    void pushDebugGroup(const char *name) override;
    void popDebugGroup() override;
    void insertDebugMarker(const char *name) override;

private:
    RenderCommandBuffer &m_buffer;
    bool m_inside_render_pass = false;
};

} // namespace Entelechy
