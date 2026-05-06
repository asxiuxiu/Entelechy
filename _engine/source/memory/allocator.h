#pragma once
#include <cstddef>
#include <cstdlib>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace Entelechy {

// Base allocator interface (static dispatch, non-virtual)
// All custom allocators must provide:
//   void* alloc(size_t size, size_t align)
//   void  free(void* ptr)
struct DefaultAllocator {
    static void* alloc(size_t size, size_t align = alignof(std::max_align_t)) {
#ifdef _WIN32
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
#ifdef _WIN32
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }
};

} // namespace Entelechy
