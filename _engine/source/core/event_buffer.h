#pragma once
#include "foundation_types.h"

namespace Entelechy {

// ------------------------------------------------------------------
// Ring-buffer based event queue (branch B fallback).
// Not used by default in Phase 4; reserved for high-frequency
// event optimizations where ECS entity spawn cost is a hotspot.
// ------------------------------------------------------------------
template<typename T, usize Capacity = 1024>
class EventBuffer {
public:
    void emit(const T& event) {
        m_events[m_tail] = event;
        m_tail = (m_tail + 1) % Capacity;
        if (m_tail == m_head) {
            m_head = (m_head + 1) % Capacity; // overwrite oldest
        }
    }

    void clear() {
        m_head = 0;
        m_tail = 0;
    }

    [[nodiscard]] bool empty() const { return m_head == m_tail; }
    [[nodiscard]] usize count() const { return (m_tail + Capacity - m_head) % Capacity; }

    template<typename F>
    void drain(F&& callback) {
        while (m_head != m_tail) {
            callback(m_events[m_head]);
            m_head = (m_head + 1) % Capacity;
        }
    }

private:
    T m_events[Capacity];
    usize m_head = 0;
    usize m_tail = 0;
};

} // namespace Entelechy
