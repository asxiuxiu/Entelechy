#include "render/rhi/render_command_buffer.h"
#include "core/allocator/allocator.h"
#include <cstdio>

namespace Entelechy
{

RenderCommandBuffer::RenderCommandBuffer(usize capacity)
    : m_buffer(static_cast<u8 *>(DefaultAllocator::alloc(capacity, AlignUp(sizeof(std::max_align_t), 64)))),
      m_capacity(capacity),
      m_offset(0)
{
}

RenderCommandBuffer::~RenderCommandBuffer()
{
    DefaultAllocator::free(m_buffer);
}

void RenderCommandBuffer::reset()
{
    m_offset = 0;
}

void *RenderCommandBuffer::allocateCommand(RenderCommandType type, u32 payloadSize)
{
    // Align the header position
    usize alignedOffset = AlignUp(m_offset, alignof(std::max_align_t));
    usize totalNeeded = sizeof(RenderCmdHeader) + AlignUp(payloadSize, alignof(std::max_align_t));

    if (alignedOffset + totalNeeded > m_capacity)
    {
        std::fprintf(stderr, "[ENSURE] RenderCommandBuffer overflow: need %zu bytes at offset %zu, capacity %zu\n",
                     static_cast<usize>(totalNeeded), static_cast<usize>(alignedOffset), static_cast<usize>(m_capacity));
        return nullptr;
    }

    // Write header
    auto *header = reinterpret_cast<RenderCmdHeader *>(m_buffer + alignedOffset);
    header->type = type;
    header->payloadSize = payloadSize;

    // Advance offset past header + aligned payload
    m_offset = alignedOffset + totalNeeded;

    // Return pointer to payload area (right after header)
    return m_buffer + alignedOffset + sizeof(RenderCmdHeader);
}

} // namespace Entelechy
