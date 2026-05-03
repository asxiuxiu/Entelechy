#include "input_queue.h"

namespace Entelechy {

InputQueue& InputQueue::instance() {
    static InputQueue s_instance;
    return s_instance;
}

void InputQueue::push(const RawInputEvent& event) {
    m_events.push_back(event);
}

void InputQueue::clear() {
    m_events.clear();
}

const std::vector<RawInputEvent>& InputQueue::events() const {
    return m_events;
}

} // namespace Entelechy
