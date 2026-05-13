#include "input_queue.h"

namespace Entelechy {

InputQueue& InputQueue::instance() {
    static InputQueue s_instance;
    return s_instance;
}

void InputQueue::push(const RawInputEvent& event) {
    m_events.pushBack(event);
}

void InputQueue::clear() {
    m_events.clear();
}

const DynamicArray<RawInputEvent>& InputQueue::events() const {
    return m_events;
}

} // namespace Entelechy
