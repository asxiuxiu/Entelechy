#pragma once
#include "foundation_types.h"
#include <cstdlib>

#if PLATFORM_WINDOWS
#include <malloc.h>
#endif

namespace Entelechy {

// Base allocator interface (static dispatch, non-virtual)
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

} // namespace Entelechy
