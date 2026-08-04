#pragma once
#include "core/foundation_types.h"

namespace Entelechy
{

// Common statistics returned by allocators.
struct AllocatorStats
{
    usize totalAllocated = 0;  // currently active bytes
    usize peakAllocated = 0;   // historical peak
    usize allocationCount = 0; // total number of allocate() calls
    usize activeCount = 0;     // currently live allocations
};

// Minimal virtual allocator interface for runtime polymorphism.
// Used by containers that need to accept different allocator backends
// (e.g. FrameArena, DebugAllocatorWrapper, or custom pools).
struct IAllocator
{
    virtual ~IAllocator() = default;
    virtual void *allocate(usize size, usize align) = 0;
    virtual void free(void *ptr) = 0;
    virtual AllocatorStats getStats() const
    {
        return {};
    }

    // Optional: try to resize an existing allocation in-place.
    // Returns nullptr if in-place resize is not possible (caller must alloc+copy+free).
    // On success, returns the (possibly same) pointer and the old pointer is invalid.
    // size is the new requested size; align is the requested alignment.
    virtual void *reallocate(void *old_ptr, usize old_size, usize new_size, usize align)
    {
        // Default: no in-place support, always returns nullptr.
        (void) old_ptr;
        (void) old_size;
        (void) new_size;
        (void) align;
        return nullptr;
    }

    // Optional: return a quantized size suitable for the allocator's bucketing.
    // Default implementation forwards to DefaultAllocator::quantizeSize.
    virtual usize quantizeSize(usize size) const;
};

} // namespace Entelechy
