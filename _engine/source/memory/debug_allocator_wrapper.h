#pragma once
#include "foundation_types.h"
#include "iallocator.h"

#if defined(_DEBUG) || defined(DEBUG)

namespace Entelechy {

// Debug allocator decorator: adds canaries, poison fill, and allocation stats.
// Completely stripped in Release builds (zero overhead).
class DebugAllocatorWrapper : public IAllocator {
public:
    explicit DebugAllocatorWrapper(IAllocator* inner);

    void* allocate(usize size, usize align) override;
    void free(void* ptr) override;
    [[nodiscard]] AllocatorStats getStats() const override;

    struct Stats {
        usize totalAllocated = 0;  // currently active bytes
        usize peakAllocated = 0;   // historical peak
        usize allocationCount = 0; // total number of allocate() calls
        usize activeCount = 0;     // currently live allocations
    };

    [[nodiscard]] Stats stats() const { return m_stats; }

private:
    IAllocator* m_inner;
    Stats m_stats;

    struct Header {
        static constexpr u64 CANARY = 0xDEADBEEFCAFEBABEull;
        u64 canary;
        usize userSize;
        void* rawPtr;
    };

    static constexpr u8 POISON_ALLOC = 0xCD;
    static constexpr u8 POISON_FREE = 0xFE;
    static constexpr usize TAIL_CANARY_SIZE = sizeof(u64);

    static void* alignForward(void* ptr, usize align);
};

} // namespace Entelechy

#endif // _DEBUG || DEBUG
