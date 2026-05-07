#pragma once
#include "foundation_types.h"

namespace Entelechy {

// Frame allocator: all temporary memory within one frame shares a large buffer.
// Call reset() at the end of the frame to reclaim everything in O(1).
class FrameArena {
public:
    explicit FrameArena(usize capacity);
    ~FrameArena();

    [[nodiscard]] void* alloc(usize size, usize align = 8);
    void reset();

    [[nodiscard]] usize capacity() const { return m_capacity; }
    [[nodiscard]] usize consumedBytes() const { return m_offset; }

private:
    u8* m_base;
    usize m_capacity;
    usize m_offset;
};

} // namespace Entelechy
