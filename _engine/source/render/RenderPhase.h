#pragma once
#include "foundation_types.h"

namespace Entelechy {

enum class RenderPhase : u8 {
    ShadowMap = 0,
    Opaque3D = 1,
    AlphaMask = 2,
    Transparent3D = 3,
    UI = 4
};

} // namespace Entelechy
