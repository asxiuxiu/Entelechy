#pragma once
#include "core/container/dynamic_array.h"
#include "core/foundation_types.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// TextureAsset — CPU-side texture data
// ------------------------------------------------------------------
// Decoded pixels, always RGBA8 (4 bytes per pixel), row-major with a
// top-left origin (stb_image convention, which also matches glTF UV
// space; any V-flip happens at GPU upload time, not here).
// Must remain default-constructible (HandleTable<T> requirement).
// ------------------------------------------------------------------
struct TextureAsset
{
    DynamicArray<u8> pixels; // width * height * 4 bytes
    u32 width = 0;
    u32 height = 0;

    [[nodiscard]] bool valid() const
    {
        return width > 0 && height > 0 && pixels.size() == static_cast<usize>(width) * height * 4;
    }
};

} // namespace Entelechy
