#pragma once
#include "world.h"
#include "command_buffer.h"
#include "dynamic_array.h"
#include "frame_arena.h"
#include <memory>

namespace Entelechy {

class System {
public:
    virtual ~System() = default;
    virtual void tick(World& world, FrameArena& arena, f32 dt) = 0;
};

class Scheduler {
public:
    void registerSystem(System* system) {
        m_systems.pushBack(system);
    }

    void tick(World& world, f32 dt) {
        m_frameArena.reset();
        for (auto* sys : m_systems) {
            sys->tick(world, m_frameArena, dt);
        }
        m_commandBuffer.apply(world);
    }

    FrameArena& frameArena() {
        return m_frameArena;
    }

    CommandBuffer& commandBuffer() {
        return m_commandBuffer;
    }

private:
    DynamicArray<System*> m_systems;
    CommandBuffer m_commandBuffer;
    FrameArena m_frameArena{1024 * 1024}; // 1 MiB per frame
};

} // namespace Entelechy
