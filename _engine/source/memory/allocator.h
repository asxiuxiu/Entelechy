#pragma once
#include <cstddef>
#include <cstdlib>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace Entelechy {

// 基础分配器接口（静态分发，非虚）
// 所有自定义分配器需提供：
//   void* alloc(size_t size, size_t align)
//   void  free(void* ptr)
struct DefaultAllocator {
    static void* alloc(size_t size, size_t align = alignof(std::max_align_t)) {
#ifdef _WIN32
        return _aligned_malloc(size, align);
#else
        return aligned_alloc(align, size);
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
