#pragma once
#include "foundation_types.h"

namespace Entelechy {

struct RawInputEvent {
    enum Type {
        KeyPress,
        KeyRelease,
        MouseMove,
        MouseButtonPress,
        MouseButtonRelease,
        WindowResize,
        Close
    };

    Type type = KeyPress;
    int keyCode = 0;
    int mouseButton = 0;
    f32 mx = 0.0f;
    f32 my = 0.0f;
    int width = 0;
    int height = 0;
};

} // namespace Entelechy
