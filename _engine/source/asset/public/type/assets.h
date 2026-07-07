#pragma once
#include "asset/handle/handle_table.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// Assets<T> — type-safe resource storage facade
// ------------------------------------------------------------------
// Each resource type gets its own Assets<T> instance.
// This provides O(1) typed access and keeps large asset data
// out of ECS components (components only store Handle<T>).
//
// Usage:
//   Assets<Mesh> meshAssets;
//   Handle<Mesh> h = meshAssets.insert(Mesh{...});
//   Mesh* m = meshAssets.get(h);
// ------------------------------------------------------------------
template <typename T>
class Assets
{
public:
    // Insert a fully-constructed asset and return its handle.
    Handle<T> insert(T value)
    {
        auto handle = m_table.allocate();
        m_table.fill(handle, std::move(value));
        return handle;
    }

    // Allocate an empty handle (for async loading).
    // The slot is reserved but contains no valid data yet.
    Handle<T> allocateEmpty()
    {
        return m_table.allocate();
    }

    // Fill a previously allocated empty handle.
    void fill(Handle<T> handle, T value)
    {
        m_table.fill(handle, std::move(value));
    }

    [[nodiscard]] T *get(Handle<T> handle)
    {
        return m_table.tryGet(handle);
    }

    [[nodiscard]] const T *get(Handle<T> handle) const
    {
        return m_table.tryGet(handle);
    }

    void remove(Handle<T> handle)
    {
        m_table.release(handle);
    }

    void incrementRef(Handle<T> handle)
    {
        m_table.incrementRef(handle);
    }

    void decrementRef(Handle<T> handle)
    {
        m_table.decrementRef(handle);
    }

    [[nodiscard]] u32 refCount(Handle<T> handle) const
    {
        return m_table.refCount(handle);
    }

    [[nodiscard]] usize count() const
    {
        return m_table.activeCount();
    }

    void clear()
    {
        m_table.clear();
    }

private:
    HandleTable<T> m_table;
};

} // namespace Entelechy
