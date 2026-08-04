#pragma once
#include "core/foundation_types.h"

namespace game
{

// Phase-1 asset IDs shared between GamePlugin (component refs in the main
// world) and main.cpp (GPU resource registration on RenderExecuteSystem).
// Replaced by the asset system + Prepare stage in Phase 2.
inline constexpr u32 CUBE_MESH_ID = 0;
inline constexpr u32 MAT_RED = 1;
inline constexpr u32 MAT_GREEN = 2;
inline constexpr u32 MAT_BLUE = 3;
inline constexpr u32 MAT_YELLOW = 4;

} // namespace game
