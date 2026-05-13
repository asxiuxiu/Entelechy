#pragma once
#include "input_event.h"
#include "base/dynamic_array.h"


namespace Entelechy {

class InputQueue {
public:
    static InputQueue& instance();

    void push(const RawInputEvent& event);
    void clear();
    const DynamicArray<RawInputEvent>& events() const;

private:
    DynamicArray<RawInputEvent> m_events;
};

} // namespace Entelechy
