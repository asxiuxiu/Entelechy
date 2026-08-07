#pragma once
#include "core/foundation_types.h"

namespace Entelechy
{

// Debug/diagnostics helper: write RGBA8 pixels (top-left origin, tightly
// packed) to a PNG file, creating parent directories as needed. Pairs with
// IRHIDevice::readbackBackbuffer() for frame capture. Returns false on
// failure (reason logged).
bool saveScreenshotPng(const char *path, const u8 *pixelsRGBA8, u32 width, u32 height);

} // namespace Entelechy
