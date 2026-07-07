#pragma once
#include "ecs/world/world.h"
#include "ecs/world/command_buffer.h"
#include "core/container/dynamic_array.h"
#include "core/memory/frame_arena.h"
#include "ecs/world/phase.h"
#include "ecs/system/system_desc.h"

namespace Entelechy
{

class System
{
public:
    virtual ~System() = default;
    virtual void tick(World &world, FrameArena &arena, f32 dt) = 0;
};

class Scheduler
{
public:
    // Legacy API: default Update phase, no dependencies.
    void registerSystem(System *system);

    // New API: full descriptor with phase, reads, writes, before, after.
    void registerSystem(const SystemDesc &desc);

    // Build dependency graph, topological sort, and ambiguity detection.
    // Must be called after all systems are registered and before first tick.
    void build();

    // Tick with fixed timestep. raw_dt is accumulated; fixed steps are executed.
    void tick(World &world, f32 raw_dt);

    // Tick exactly once with the given dt (for Inspector "Tick Once" button).
    void tickOnce(World &world, f32 dt);

    FrameArena &frameArena();
    CommandBuffer &commandBuffer();

    [[nodiscard]] u64 currentFrame() const
    {
        return m_current_frame;
    }

private:
    static constexpr f32 FIXED_DT = 1.0f / 60.0f;

    void tickFixed(World &world, f32 dt);
    void detectAmbiguities();

    struct PhaseGroup
    {
        u8 phaseIndex = 0;
        DynamicArray<SystemDesc *> systems;
    };

    DynamicArray<SystemDesc> m_systems;
    DynamicArray<PhaseGroup> m_phase_groups;
    CommandBuffer m_command_buffer;
    FrameArena m_frame_arena{1024 * 1024}; // 1 MiB per frame
    f32 m_accumulator = 0.0f;
    u64 m_current_frame = 0;
    bool m_built = false;
};

} // namespace Entelechy
