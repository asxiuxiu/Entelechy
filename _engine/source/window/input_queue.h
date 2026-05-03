#pragma once
#include "input_event.h"
#include <vector>

namespace Entelechy {

class InputQueue {
public:
    static InputQueue& instance();

    void push(const RawInputEvent& event);
    void clear();
    const std::vector<RawInputEvent>& events() const;

private:
    std::vector<RawInputEvent> m_events;
};

} // namespace Entelechy
