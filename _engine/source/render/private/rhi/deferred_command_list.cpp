#include "render/rhi/deferred_command_list.h"
#include <cstring>

namespace Entelechy
{

DeferredCommandList::DeferredCommandList(RenderCommandBuffer &buffer) : m_buffer(buffer) {}

void DeferredCommandList::begin()
{
    m_inside_render_pass = false;
}

void DeferredCommandList::end()
{
    if (m_inside_render_pass)
    {
        endRenderPass();
    }
}

// ------------------------------------------------------------------
// Render pass
// ------------------------------------------------------------------
void DeferredCommandList::beginRenderPass(const RenderPassDesc &desc)
{
    if (m_inside_render_pass)
    {
        endRenderPass();
    }
    m_inside_render_pass = true;

    auto *payload = static_cast<CmdBeginRenderPass *>(
        m_buffer.allocateCommand(RenderCommandType::BeginRenderPass, sizeof(CmdBeginRenderPass)));
    if (payload)
    {
        payload->desc = desc;
    }
}

void DeferredCommandList::endRenderPass()
{
    m_inside_render_pass = false;
    m_buffer.allocateCommand(RenderCommandType::EndRenderPass, 0);
}

// ------------------------------------------------------------------
// Viewport and scissor
// ------------------------------------------------------------------
void DeferredCommandList::setViewport(f32 x, f32 y, f32 w, f32 h)
{
    auto *payload = static_cast<CmdSetViewport *>(
        m_buffer.allocateCommand(RenderCommandType::SetViewport, sizeof(CmdSetViewport)));
    if (payload)
    {
        payload->x = x;
        payload->y = y;
        payload->w = w;
        payload->h = h;
    }
}

void DeferredCommandList::setScissor(u32 x, u32 y, u32 w, u32 h)
{
    auto *payload = static_cast<CmdSetScissor *>(
        m_buffer.allocateCommand(RenderCommandType::SetScissor, sizeof(CmdSetScissor)));
    if (payload)
    {
        payload->x = x;
        payload->y = y;
        payload->w = w;
        payload->h = h;
    }
}

// ------------------------------------------------------------------
// Pipeline and resource binding
// ------------------------------------------------------------------
void DeferredCommandList::bindPipeline(RHIPipelineState *pso)
{
    auto *payload = static_cast<CmdBindPipeline *>(
        m_buffer.allocateCommand(RenderCommandType::BindPipeline, sizeof(CmdBindPipeline)));
    if (payload)
    {
        payload->pso = pso;
    }
}

void DeferredCommandList::bindVertexBuffer(RHIBuffer *buffer, u32 slot, u32 offset)
{
    auto *payload = static_cast<CmdBindVertexBuffer *>(
        m_buffer.allocateCommand(RenderCommandType::BindVertexBuffer, sizeof(CmdBindVertexBuffer)));
    if (payload)
    {
        payload->buffer = buffer;
        payload->slot = slot;
        payload->offset = offset;
    }
}

void DeferredCommandList::bindIndexBuffer(RHIBuffer *buffer, u32 offset)
{
    auto *payload = static_cast<CmdBindIndexBuffer *>(
        m_buffer.allocateCommand(RenderCommandType::BindIndexBuffer, sizeof(CmdBindIndexBuffer)));
    if (payload)
    {
        payload->buffer = buffer;
        payload->offset = offset;
    }
}

// ------------------------------------------------------------------
// Draw commands
// ------------------------------------------------------------------
void DeferredCommandList::drawIndexed(u32 indexCount, u32 startIndex, i32 baseVertex)
{
    auto *payload = static_cast<CmdDrawIndexed *>(
        m_buffer.allocateCommand(RenderCommandType::DrawIndexed, sizeof(CmdDrawIndexed)));
    if (payload)
    {
        payload->indexCount = indexCount;
        payload->startIndex = startIndex;
        payload->baseVertex = baseVertex;
    }
}

void DeferredCommandList::draw(u32 vertexCount, u32 startVertex)
{
    auto *payload = static_cast<CmdDraw *>(
        m_buffer.allocateCommand(RenderCommandType::Draw, sizeof(CmdDraw)));
    if (payload)
    {
        payload->vertexCount = vertexCount;
        payload->startVertex = startVertex;
    }
}

// ------------------------------------------------------------------
// Resource barriers (variable-length: count + inline BarrierDesc array)
// ------------------------------------------------------------------
void DeferredCommandList::resourceBarrier(const BarrierDesc *barriers, u32 count)
{
    u32 payloadSize = sizeof(CmdResourceBarrier) + count * sizeof(BarrierDesc);
    auto *payload = static_cast<CmdResourceBarrier *>(
        m_buffer.allocateCommand(RenderCommandType::ResourceBarrier, payloadSize));
    if (payload)
    {
        payload->count = count;
        if (count > 0 && barriers)
        {
            auto *inlineBarriers = reinterpret_cast<BarrierDesc *>(
                reinterpret_cast<u8 *>(payload) + sizeof(CmdResourceBarrier));
            std::memcpy(inlineBarriers, barriers, count * sizeof(BarrierDesc));
        }
    }
}

// ------------------------------------------------------------------
// Clear
// ------------------------------------------------------------------
void DeferredCommandList::clearRenderTarget(u32 attachmentIndex, const f32 color[4])
{
    auto *payload = static_cast<CmdClearRenderTarget *>(
        m_buffer.allocateCommand(RenderCommandType::ClearRenderTarget, sizeof(CmdClearRenderTarget)));
    if (payload)
    {
        payload->attachmentIndex = attachmentIndex;
        payload->color[0] = color[0];
        payload->color[1] = color[1];
        payload->color[2] = color[2];
        payload->color[3] = color[3];
    }
}

// ------------------------------------------------------------------
// Uniform setters
// ------------------------------------------------------------------
void DeferredCommandList::setUniformFloat(StringId name, f32 value)
{
    auto *payload = static_cast<CmdSetUniformFloat *>(
        m_buffer.allocateCommand(RenderCommandType::SetUniformFloat, sizeof(CmdSetUniformFloat)));
    if (payload)
    {
        payload->name = name;
        payload->value = value;
    }
}

void DeferredCommandList::setUniformInt(StringId name, i32 value)
{
    auto *payload = static_cast<CmdSetUniformInt *>(
        m_buffer.allocateCommand(RenderCommandType::SetUniformInt, sizeof(CmdSetUniformInt)));
    if (payload)
    {
        payload->name = name;
        payload->value = value;
    }
}

void DeferredCommandList::setUniformVec2(StringId name, const f32 *value)
{
    auto *payload = static_cast<CmdSetUniformVec2 *>(
        m_buffer.allocateCommand(RenderCommandType::SetUniformVec2, sizeof(CmdSetUniformVec2)));
    if (payload && value)
    {
        payload->name = name;
        payload->value[0] = value[0];
        payload->value[1] = value[1];
    }
}

void DeferredCommandList::setUniformVec3(StringId name, const f32 *value)
{
    auto *payload = static_cast<CmdSetUniformVec3 *>(
        m_buffer.allocateCommand(RenderCommandType::SetUniformVec3, sizeof(CmdSetUniformVec3)));
    if (payload && value)
    {
        payload->name = name;
        payload->value[0] = value[0];
        payload->value[1] = value[1];
        payload->value[2] = value[2];
    }
}

void DeferredCommandList::setUniformVec4(StringId name, const f32 *value)
{
    auto *payload = static_cast<CmdSetUniformVec4 *>(
        m_buffer.allocateCommand(RenderCommandType::SetUniformVec4, sizeof(CmdSetUniformVec4)));
    if (payload && value)
    {
        payload->name = name;
        payload->value[0] = value[0];
        payload->value[1] = value[1];
        payload->value[2] = value[2];
        payload->value[3] = value[3];
    }
}

void DeferredCommandList::setUniformMat3(StringId name, const f32 *value, bool transpose)
{
    auto *payload = static_cast<CmdSetUniformMat3 *>(
        m_buffer.allocateCommand(RenderCommandType::SetUniformMat3, sizeof(CmdSetUniformMat3)));
    if (payload && value)
    {
        payload->name = name;
        payload->transpose = transpose;
        std::memcpy(payload->value, value, 9 * sizeof(f32));
    }
}

void DeferredCommandList::setUniformMat4(StringId name, const f32 *value, bool transpose)
{
    auto *payload = static_cast<CmdSetUniformMat4 *>(
        m_buffer.allocateCommand(RenderCommandType::SetUniformMat4, sizeof(CmdSetUniformMat4)));
    if (payload && value)
    {
        payload->name = name;
        payload->transpose = transpose;
        std::memcpy(payload->value, value, 16 * sizeof(f32));
    }
}

void DeferredCommandList::bindTexture(u32 slot, RHITexture *texture)
{
    auto *payload = static_cast<CmdBindTexture *>(
        m_buffer.allocateCommand(RenderCommandType::BindTexture, sizeof(CmdBindTexture)));
    if (payload)
    {
        payload->slot = slot;
        payload->texture = texture;
    }
}

// ------------------------------------------------------------------
// Constant buffer / push constants
// ------------------------------------------------------------------
void DeferredCommandList::bindConstantBuffer(u32 binding, RHIBuffer *buffer, u32 offset, u32 size)
{
    auto *payload = static_cast<CmdBindConstantBuffer *>(
        m_buffer.allocateCommand(RenderCommandType::BindConstantBuffer, sizeof(CmdBindConstantBuffer)));
    if (payload)
    {
        payload->binding = binding;
        payload->buffer = buffer;
        payload->offset = offset;
        payload->size = size;
    }
}

void DeferredCommandList::setPushConstants(u32 offset, const void *data, u32 size)
{
    u32 payloadSize = sizeof(CmdSetPushConstants) + size;
    auto *payload = static_cast<CmdSetPushConstants *>(
        m_buffer.allocateCommand(RenderCommandType::SetPushConstants, payloadSize));
    if (payload)
    {
        payload->offset = offset;
        payload->dataSize = size;
        if (size > 0 && data)
        {
            std::memcpy(reinterpret_cast<u8 *>(payload) + sizeof(CmdSetPushConstants), data, size);
        }
    }
}

// ------------------------------------------------------------------
// Debug markers (variable-length: nameLength + inline string)
// ------------------------------------------------------------------
void DeferredCommandList::pushDebugGroup(const char *name)
{
    u32 nameLen = name ? static_cast<u32>(std::strlen(name) + 1) : 0;
    u32 payloadSize = sizeof(CmdDebugString) + nameLen;
    auto *payload = static_cast<CmdDebugString *>(
        m_buffer.allocateCommand(RenderCommandType::PushDebugGroup, payloadSize));
    if (payload)
    {
        payload->nameLength = nameLen;
        if (nameLen > 0)
        {
            std::memcpy(reinterpret_cast<u8 *>(payload) + sizeof(CmdDebugString), name, nameLen);
        }
    }
}

void DeferredCommandList::popDebugGroup()
{
    m_buffer.allocateCommand(RenderCommandType::PopDebugGroup, 0);
}

void DeferredCommandList::insertDebugMarker(const char *name)
{
    u32 nameLen = name ? static_cast<u32>(std::strlen(name) + 1) : 0;
    u32 payloadSize = sizeof(CmdDebugString) + nameLen;
    auto *payload = static_cast<CmdDebugString *>(
        m_buffer.allocateCommand(RenderCommandType::InsertDebugMarker, payloadSize));
    if (payload)
    {
        payload->nameLength = nameLen;
        if (nameLen > 0)
        {
            std::memcpy(reinterpret_cast<u8 *>(payload) + sizeof(CmdDebugString), name, nameLen);
        }
    }
}

} // namespace Entelechy
