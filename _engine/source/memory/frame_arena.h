#pragma once
#include <cstdint>
#include <cstddef>

namespace Entelechy {

// Frame allocator: all temporary memory within one frame shares a large buffer.
// Call reset() at the end of the frame to reclaim everything in O(1).
class FrameArena {
public:
    explicit FrameArena(size_t capacity);
    ~FrameArena();

    [[nodiscard]] void* alloc(size_t size, size_t align = 8);
    void reset();

    [[nodiscard]] size_t capacity() const { return m_capacity; }
    [[nodiscard]] size_t consumedBytes() const { return m_offset; }

private:
    uint8_t* m_base;
    size_t m_capacity;
    size_t m_offset;
};

} // namespace Entelechy
