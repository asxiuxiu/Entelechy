#pragma once
#include "core/foundation_types.h"
#include "core/allocator/iallocator.h"
#include <cstdlib>

#if ENTELECHY_USE_MIMALLOC
#include <mimalloc.h>
#endif

#if PLATFORM_WINDOWS
#include <malloc.h>
#endif

namespace Entelechy {

// Base allocator interface (static dispatch, non-virtual).
// All custom allocators must provide:
//   void* alloc(usize size, usize align)
//   void  free(void* ptr)
struct DefaultAllocator {
    static void* alloc(usize size, usize align = alignof(std::max_align_t)) {
#if ENTELECHY_USE_MIMALLOC
        return mi_malloc_aligned(size, align);
#else
#if PLATFORM_WINDOWS
        return _aligned_malloc(size, align);
#else
        if (align <= alignof(std::max_align_t)) {
            return std::malloc(size);
        }
        void* ptr = nullptr;
        posix_memalign(&ptr, align, size);
        return ptr;
#endif
#endif
    }

    static void free(void* ptr) {
#if ENTELECHY_USE_MIMALLOC
        mi_free(ptr);
#else
#if PLATFORM_WINDOWS
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
#endif
    }

    // Quantize requested size to a bucket size to reduce internal fragmentation.
    // Mimalloc uses size classes; this approximation prevents frequent
    // re-allocation when capacities grow slightly.
    static usize quantizeSize(usize size) {
        if (size <= 16) return 16;
        if (size <= 32) return 32;
        if (size <= 64) return 64;
        if (size <= 128) return 128;
        if (size <= 256) return 256;
        if (size <= 512) return 512;
        if (size <= 1024) return 1024;
        if (size <= 2048) return 2048;
        if (size <= 4096) return 4096;
        if (size <= 8192) return 8192;
        if (size <= 16384) return 16384;
        if (size <= 32768) return 32768;
        if (size <= 65536) return 65536;
        if (size <= 131072) return 131072;
        if (size <= 262144) return 262144;
        if (size <= 524288) return 524288;
        if (size <= 1048576) return 1048576;
        // For larger sizes, round up to next power of two
        usize p = 1048576;
        while (p < size) p <<= 1;
        return p;
    }
};

// Virtual wrapper for runtime polymorphism (decorators, container injection).
struct DefaultAllocatorV : IAllocator {
    void* allocate(usize size, usize align) override {
        return DefaultAllocator::alloc(size, align);
    }
    void free(void* ptr) override {
        DefaultAllocator::free(ptr);
    }
    [[nodiscard]] AllocatorStats getStats() const override { return {}; }
};

// Global allocator accessor.
// In Debug/Development builds this returns a DebugAllocatorWrapper.
// In Release builds this returns DefaultAllocatorV directly (zero overhead).
IAllocator* GetGlobalAllocator();

// Aggregate stats from the global allocator (convenience for ImGui panels).
[[nodiscard]] inline AllocatorStats GetGlobalAllocatorStats() {
    return GetGlobalAllocator()->getStats();
}

} // namespace Entelechy
