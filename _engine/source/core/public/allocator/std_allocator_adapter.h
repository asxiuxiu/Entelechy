#pragma once
#include "core/foundation_types.h"
#include <cstddef>
#include <type_traits>

namespace Entelechy {

// Adapter that wraps an engine allocator (static dispatch: alloc(size, align) / free(ptr))
// into a C++ standard allocator interface for use with std:: containers.
//
// Requirements on EngineAllocatorT:
//   static void* alloc(usize size, usize align)
//   static void  free(void* ptr)
//
// This adapter is stateless and always equal, matching the behavior of
// engine allocators which are typically global or thread-local singletons.
template<typename T, typename EngineAllocatorT>
class StdAllocatorAdapter {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;
    using is_always_equal = std::true_type;

    StdAllocatorAdapter() = default;
    ~StdAllocatorAdapter() = default;

    template<typename U>
    StdAllocatorAdapter(const StdAllocatorAdapter<U, EngineAllocatorT>&) noexcept {}

    T* allocate(std::size_t n) {
        return static_cast<T*>(EngineAllocatorT::alloc(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, std::size_t) noexcept {
        EngineAllocatorT::free(p);
    }

    template<typename U>
    struct rebind {
        using other = StdAllocatorAdapter<U, EngineAllocatorT>;
    };

    bool operator==(const StdAllocatorAdapter&) const noexcept { return true; }
    bool operator!=(const StdAllocatorAdapter&) const noexcept { return false; }
};

} // namespace Entelechy
