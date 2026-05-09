#pragma once
#include "foundation_types.h"
#include "iallocator.h"
#include <cstdlib>

#if PLATFORM_WINDOWS
#include <malloc.h>
#endif

namespace Entelechy {

// Base allocator interface (static dispatch, non-virtual).
// All custom allocators must provide:
//   void* alloc(size_t size, size_t align)
//   void  free(void* ptr)
struct DefaultAllocator {
    static void* alloc(usize size, usize align = alignof(std::max_align_t)) {
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
    }

    static void free(void* ptr) {
#if PLATFORM_WINDOWS
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
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
