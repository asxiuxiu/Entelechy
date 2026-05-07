#include "frame_arena.h"
#include "foundation_types.h"
#include <cstdlib>

namespace Entelechy {

FrameArena::FrameArena(usize capacity)
    : m_base(static_cast<u8*>(std::malloc(capacity)))
    , m_capacity(capacity)
    , m_offset(0) {
}

FrameArena::~FrameArena() {
    std::free(m_base);
}

void* FrameArena::alloc(usize size, usize align) {
    usize mask = align - 1;
    usize padded = (m_offset + mask) & ~mask;
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
