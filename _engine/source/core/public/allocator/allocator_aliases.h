#pragma once
#include "core/allocator/allocator.h"

namespace Entelechy
{

// Universal persistent objects: route through global allocator (Mimalloc).
using HeapAllocator = DefaultAllocator;

// ECS component storage columns (Column<T> data buffers).
// Currently falls back to DefaultAllocator; may switch to Chunk allocator in future.
using ColumnAllocator = DefaultAllocator;

// ECS component array shells (ComponentArray<T> instances).
// Very few in number (= component type count); Phase 2.2 may replace with ObjectPool.
using ComponentArrayAllocator = DefaultAllocator;

} // namespace Entelechy
