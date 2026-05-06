#include "frame_arena.h"
#include <cstdlib>

namespace Entelechy {

FrameArena::FrameArena(size_t capacity)
    : m_base(static_cast<uint8_t*>(std::malloc(capacity)))
    , m_capacity(capacity)
    , m_offset(0) {
}

FrameArena::~FrameArena() {
    std::free(m_base);
}

void* FrameArena::alloc(size_t size, size_t align) {
    size_t mask = align - 1;
    size_t padded = (m_offset + mask) & ~mask;
    if (padded + size > m_capacity) {
        return nullptr; // Overflow, no fallback
    }
    m_offset = padded + size;
    return m_base + padded;
}

void FrameArena::reset() {
    m_offset = 0;
}

} // namespace Entelechy
