#pragma once
#include "world.h"
#include <vector>
#include <memory>

namespace Entelechy {

class System {
public:
    virtual ~System() = default;
    virtual void tick(World& world, float dt) = 0;
};

class Scheduler {
public:
    void registerSystem(System* system) {
        m_systems.push_back(system);
    }

    void tick(World& world, float dt) {
        for (auto* sys : m_systems) {
            sys->tick(world, dt);
        }
    }

private:
    std::vector<System*> m_systems;
};

} // namespace Entelechy
