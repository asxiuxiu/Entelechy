#pragma once
#include "core/foundation_types.h"
#include "core/math/align.h"
#include "render/rhi/rhi_types.h"
#include "render/rhi/rhi_resources.h"
#include "render/rhi/rhi_pipeline.h"
#include "core/string/string_id.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// RenderCommandType — enum covering every IRHICommandList operation.
// Serialized as the first field of each command header in the buffer.
// ------------------------------------------------------------------
enum class RenderCommandType : u32
{
    BeginRenderPass,
    EndRenderPass,
    SetViewport,
    SetScissor,
    BindPipeline,
    BindVertexBuffer,
    BindIndexBuffer,
    DrawIndexed,
    Draw,
    ResourceBarrier,
    ClearRenderTarget,
    SetUniformFloat,
    SetUniformInt,
    SetUniformVec2,
    SetUniformVec3,
    SetUniformVec4,
    SetUniformMat3,
    SetUniformMat4,
    BindTexture,
    BindConstantBuffer,
    SetPushConstants,
    PushDebugGroup,
    PopDebugGroup,
    InsertDebugMarker,
};

// ------------------------------------------------------------------
// RenderCmdHeader — fixed-size prefix before every command payload.
// ------------------------------------------------------------------
struct RenderCmdHeader
{
    RenderCommandType type;
    u32 payloadSize; // bytes following this header (not including alignment padding)
};

STATIC_ASSERT(sizeof(RenderCmdHeader) == 8, "RenderCmdHeader must be 8 bytes");

// ------------------------------------------------------------------
// Payload structs — one per command type. All POD / trivially copyable.
// ------------------------------------------------------------------

struct CmdBeginRenderPass
{
    RenderPassDesc desc;
};

// CmdEndRenderPass: no payload (payloadSize == 0)

struct CmdSetViewport
{
    f32 x, y, w, h;
};

struct CmdSetScissor
{
    u32 x, y, w, h;
};

struct CmdBindPipeline
{
    RHIPipelineState *pso;
};

struct CmdBindVertexBuffer
{
    RHIBuffer *buffer;
    u32 slot;
    u32 offset;
};

struct CmdBindIndexBuffer
{
    RHIBuffer *buffer;
    u32 offset;
};

struct CmdDrawIndexed
{
    u32 indexCount;
    u32 startIndex;
    i32 baseVertex;
};

struct CmdDraw
{
    u32 vertexCount;
    u32 startVertex;
};

// ResourceBarrier: variable-length. Fixed part is count, followed by
// `count` inline BarrierDesc copies.
struct CmdResourceBarrier
{
    u32 count;
    // BarrierDesc barriers[count]; // inline after this struct
};

struct CmdClearRenderTarget
{
    u32 attachmentIndex;
    f32 color[4];
};

struct CmdSetUniformFloat
{
    StringId name;
    f32 value;
};

struct CmdSetUniformInt
{
    StringId name;
    i32 value;
};

struct CmdSetUniformVec2
{
    StringId name;
    f32 value[2];
};

struct CmdSetUniformVec3
{
    StringId name;
    f32 value[3];
};

struct CmdSetUniformVec4
{
    StringId name;
    f32 value[4];
};

struct CmdSetUniformMat3
{
    StringId name;
    bool transpose;
    f32 value[9];
};

struct CmdSetUniformMat4
{
    StringId name;
    bool transpose;
    f32 value[16];
};

struct CmdBindTexture
{
    u32 slot;
    RHITexture *texture;
};

struct CmdBindConstantBuffer
{
    u32 binding;
    RHIBuffer *buffer;
    u32 offset;
    u32 size;
};

// SetPushConstants: variable-length. Fixed part is offset+size, followed
// by `size` bytes of inline data.
struct CmdSetPushConstants
{
    u32 offset;
    u32 dataSize;
    // u8 data[dataSize]; // inline after this struct
};

// Debug string commands: variable-length. Fixed part is nameLength,
// followed by `nameLength` bytes (including null terminator).
struct CmdDebugString
{
    u32 nameLength; // includes null terminator
    // char name[nameLength]; // inline after this struct
};

// CmdPopDebugGroup: no payload (payloadSize == 0)

// ------------------------------------------------------------------
// RenderCommandBuffer — contiguous bump-allocated byte buffer for
// recording render commands. Reset each frame in O(1).
//
// Unlike FrameArena, this buffer does NOT fall back to heap on overflow.
// Commands must be contiguous for sequential replay by translators.
// Overflow returns nullptr and logs an error.
// ------------------------------------------------------------------
class RenderCommandBuffer
{
public:
    explicit RenderCommandBuffer(usize capacity = 4 * 1024 * 1024);
    ~RenderCommandBuffer();

    RenderCommandBuffer(const RenderCommandBuffer &) = delete;
    RenderCommandBuffer &operator=(const RenderCommandBuffer &) = delete;

    // Reset the buffer for a new frame. O(1), no deallocation.
    void reset();

    // Allocate space for a command header + payload. Returns pointer to
    // the payload area (right after the header). The header is written
    // automatically with the given type and payloadSize.
    // Returns nullptr on overflow (buffer full).
    void *allocateCommand(RenderCommandType type, u32 payloadSize);

    // Raw buffer access for translators.
    const u8 *data() const
    {
        return m_buffer;
    }
    usize size() const
    {
        return m_offset;
    }
    usize capacity() const
    {
        return m_capacity;
    }

private:
    u8 *m_buffer;
    usize m_capacity;
    usize m_offset;
};

} // namespace Entelechy
