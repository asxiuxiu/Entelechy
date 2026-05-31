#pragma once
#include "core/foundation_types.h"

namespace Entelechy {

// Common statistics returned by allocators.
struct AllocatorStats {
    usize totalAllocated = 0;  // currently active bytes
    usize peakAllocated = 0;   // historical peak
    usize allocationCount = 0; // total number of allocate() calls
    usize activeCount = 0;     // currently live allocations
};

// Minimal virtual allocator interface for runtime polymorphism.
// Used by containers that need to accept different allocator backends
// (e.g. FrameArena, DebugAllocatorWrapper, or custom pools).
struct IAllocator {
    virtual ~IAllocator() = default;
    virtual void* allocate(usize size, usize align) = 0;
    virtual void free(void* ptr) = 0;
    virtual AllocatorStats getStats() const { return {}; }
};

} // namespace Entelechy
