#include "core/memory/frame_arena.h"
#include "core/allocator/allocator.h"
#include <utility>

namespace Entelechy
{

// ------------------------------------------------------------------
// MemMark
// ------------------------------------------------------------------
MemMark::MemMark(FrameArena *a) : savedOffset(a->consumedBytes()), arena(a), rolledBack(false) {}

MemMark::~MemMark()
{
    if (!rolledBack && arena)
    {
        arena->rollback(*this);
    }
}

void MemMark::rollback()
{
    if (!rolledBack && arena)
    {
        arena->rollback(*this);
        rolledBack = true;
    }
}

// ------------------------------------------------------------------
// FrameArena
// ------------------------------------------------------------------
FrameArena::FrameArena(usize capacity)
    : m_base(static_cast<u8 *>(DefaultAllocator::alloc(capacity, 64))),
      m_capacity(capacity),
      m_offset(0),
      m_overflow_head(nullptr)
{
}

FrameArena::~FrameArena()
{
    reset();
    DefaultAllocator::free(m_base);
}

void *FrameArena::allocate(usize size, usize align)
{
    usize mask = align - 1;
    usize padded = (m_offset + mask) & ~mask;

    if (padded + size <= m_capacity)
    {
        m_offset = padded + size;
        m_stats.allocationCount++;
        return m_base + padded;
    }

    // Try to fit into the newest overflow block (head of list)
    if (m_overflow_head)
    {
        usize op = (m_overflow_head->offset + mask) & ~mask;
        if (op + size <= m_overflow_head->capacity)
        {
            m_overflow_head->offset = op + size;
            m_stats.allocationCount++;
            return m_overflow_head->memory + op;
        }
    }

    // Allocate a new overflow block
    usize blockSize = size > 4096 ? size : 4096;
    blockSize = (blockSize + mask) & ~mask;
    if (blockSize < size)
        return nullptr;

    u8 *mem = static_cast<u8 *>(DefaultAllocator::alloc(blockSize, align));
    if (!mem)
        return nullptr;

    OverflowBlock *block =
        static_cast<OverflowBlock *>(DefaultAllocator::alloc(sizeof(OverflowBlock), alignof(OverflowBlock)));
    if (!block)
    {
        DefaultAllocator::free(mem);
        return nullptr;
    }

    block->memory = mem;
    block->capacity = blockSize;
    block->offset = size;
    block->next = m_overflow_head;
    m_overflow_head = block;
    m_stats.allocationCount++;

    return mem;
}

void FrameArena::reset()
{
    m_offset = 0;
    OverflowBlock *block = m_overflow_head;
    while (block)
    {
        OverflowBlock *next = block->next;
        DefaultAllocator::free(block->memory);
        DefaultAllocator::free(block);
        block = next;
    }
    m_overflow_head = nullptr;
}

void FrameArena::rollback(const MemMark &mark)
{
    CHECK(mark.arena == this);

    if (mark.savedOffset <= m_capacity)
    {
        // Rollback within main block: free all overflow and reset main offset
        m_offset = mark.savedOffset;
        OverflowBlock *block = m_overflow_head;
        while (block)
        {
            OverflowBlock *next = block->next;
            DefaultAllocator::free(block->memory);
            DefaultAllocator::free(block);
            block = next;
        }
        m_overflow_head = nullptr;
    }
    else
    {
        // Overflow rollback not precisely supported in Phase A.
        // Safe fallback: free all overflow and clamp to capacity.
        OverflowBlock *block = m_overflow_head;
        while (block)
        {
            OverflowBlock *next = block->next;
            DefaultAllocator::free(block->memory);
            DefaultAllocator::free(block);
            block = next;
        }
        m_overflow_head = nullptr;
        m_offset = m_capacity; // main block was already full
    }
}

usize FrameArena::consumedBytes() const
{
    usize total = m_offset;
    for (OverflowBlock *b = m_overflow_head; b; b = b->next)
    {
        total += b->offset;
    }
    return total;
}

AllocatorStats FrameArena::getStats() const
{
    AllocatorStats result = m_stats;
    result.totalAllocated = consumedBytes();
    if (result.totalAllocated > m_stats.peakAllocated)
    {
        m_stats.peakAllocated = result.totalAllocated;
    }
    result.peakAllocated = m_stats.peakAllocated;
    // activeCount is not tracked per-allocation for arenas
    result.activeCount = 0;
    return result;
}

void FrameArena::swap(FrameArena &other) noexcept
{
    using std::swap;
    swap(m_base, other.m_base);
    swap(m_capacity, other.m_capacity);
    swap(m_offset, other.m_offset);
    swap(m_overflow_head, other.m_overflow_head);
    swap(m_stats, other.m_stats);
}

} // namespace Entelechy
