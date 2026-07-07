#pragma once
#include "asset/type/asset_types.h"
#include "core/foundation_types.h"
#include <functional>

namespace Entelechy
{

// ------------------------------------------------------------------
// Handle<T> — type-safe asset handle (index + generation)
// ------------------------------------------------------------------
// Components should store Handle<T> instead of raw pointers.
// 8 bytes POD, copyable without atomic overhead.
//
// ABA protection: generation is incremented every time a slot is
// recycled. Old handles will fail tryGet().
// ------------------------------------------------------------------
template <typename T>
struct Handle
{
    u32 index = INVALID_ASSET_TYPE_ID;
    u32 generation = INVALID_ASSET_TYPE_ID;

    [[nodiscard]] bool valid() const
    {
        return index != INVALID_ASSET_TYPE_ID && generation != INVALID_ASSET_TYPE_ID;
    }

    bool operator==(const Handle &other) const = default;
};

} // namespace Entelechy

namespace std
{
template <typename T>
struct hash<Entelechy::Handle<T>>
{
    usize operator()(const Entelechy::Handle<T> &h) const noexcept
    {
        return (static_cast<usize>(h.index) << 32) | static_cast<usize>(h.generation);
    }
};
} // namespace std
