#pragma once
#include "core/foundation_types.h"
#include "core/allocator/iallocator.h"

namespace Entelechy {

class FrameArena;

// Nested scope marker for FrameArena. Supports RAII auto-rollback
// and explicit rollback(). Rolling back twice is a no-op.
struct MemMark {
    usize savedOffset = 0;
    FrameArena* arena = nullptr;
    bool rolledBack = false;

    explicit MemMark(FrameArena* a);
    ~MemMark();

    void rollback();
};

// Frame allocator: all temporary memory within one frame shares a large buffer.
// Call reset() at the end of the frame to reclaim everything in O(1).
// Overflow allocations fall back to the default heap allocator; reset() frees them.
//
// Allocation decision guide: see ALLOCATOR_GUIDE.md
class FrameArena : public IAllocator {
public:
    explicit FrameArena(usize capacity);
    ~FrameArena();

    // IAllocator interface
    void* allocate(usize size, usize align) override;
    void free(void* ptr) override { /* no-op for arena allocations */ }
    [[nodiscard]] AllocatorStats getStats() const override;

    // Legacy convenience alias
    [[nodiscard]] void* alloc(usize size, usize align = 8) { return allocate(size, align); }

    void reset();
    void rollback(const MemMark& mark);

    [[nodiscard]] usize capacity() const { return m_capacity; }
    [[nodiscard]] usize consumedBytes() const;

    // Swap internal state with another arena (for double-buffering)
    void swap(FrameArena& other) noexcept;

private:
    struct OverflowBlock {
        u8* memory;
        usize capacity;
        usize offset;
        OverflowBlock* next;
    };

    u8* m_base;
    usize m_capacity;
    usize m_offset;
    OverflowBlock* m_overflow_head;
    mutable AllocatorStats m_stats;
};

// Ring of N arenas for double-buffering / multi-threaded frame overlap.
// Phase A: interface only; not yet wired into Scheduler.
template <usize N>
class FrameArenaRing {
public:
    explicit FrameArenaRing(usize capacityPerArena)
        : m_index(0) {
        for (usize i = 0; i < N; ++i) {
            // Placement-new to avoid default ctor then assignment
            new (&m_arenas[i]) FrameArena(capacityPerArena);
        }
    }

    ~FrameArenaRing() {
        for (usize i = 0; i < N; ++i) {
            m_arenas[i].~FrameArena();
        }
    }

    FrameArena& current() { return m_arenas[m_index]; }
    const FrameArena& current() const { return m_arenas[m_index]; }

    void advance() { m_index = (m_index + 1) % N; }

    void resetAll() {
        for (usize i = 0; i < N; ++i) {
            m_arenas[i].reset();
        }
    }

private:
    alignas(FrameArena) u8 m_storage[N * sizeof(FrameArena)];
    FrameArena* m_arenas = reinterpret_cast<FrameArena*>(m_storage);
    usize m_index;
};

} // namespace Entelechy
