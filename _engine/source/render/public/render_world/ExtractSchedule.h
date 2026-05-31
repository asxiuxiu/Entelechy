#pragma once
#include "ecs/world/world.h"
#include "core/memory/frame_arena.h"
#include "core/container/dynamic_array.h"

namespace Entelechy {

// IExtractSystem — custom interface for systems that copy data from main world
// to render world. Extract systems are read-only on the main world and write-only
// on the render world, making them safe to parallelise later.
class IExtractSystem {
public:
    virtual ~IExtractSystem() = default;
    virtual void extract(const World& mainWorld, World& renderWorld, FrameArena& arena, f32 dt) = 0;
};

// ExtractSchedule — runs all registered extract systems in order.
// Phase-1 simplification: single-threaded, sequential execution.
class ExtractSchedule {
public:
    void registerSystem(IExtractSystem* system);
    void run(const World& mainWorld, World& renderWorld, FrameArena& arena, f32 dt);
    void clear();

private:
    DynamicArray<IExtractSystem*> m_systems;
};

} // namespace Entelechy
