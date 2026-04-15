#pragma once
#include <cstdint>
#include <cstddef>

namespace Entelechy {

// 帧分配器：一帧内所有临时内存共享一块大缓冲区，
// 帧结束时 reset()，O(1) 全部回收。
class FrameArena {
public:
    explicit FrameArena(size_t capacity);
    ~FrameArena();

    void* alloc(size_t size, size_t align = 8);
    void reset();

    size_t capacity() const { return m_capacity; }
    size_t consumedBytes() const { return m_offset; }

private:
    uint8_t* m_base;
    size_t m_capacity;
    size_t m_offset;
};

} // namespace Entelechy
