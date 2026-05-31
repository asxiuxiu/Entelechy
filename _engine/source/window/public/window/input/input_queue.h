#pragma once
#include "window/input/input_event.h"
#include "core/container/dynamic_array.h"


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
