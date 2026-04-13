#pragma once
#include "world.h"
#include "scheduler.h"
#include "movement_system.h"
#include "tool_registry.h"
#include <string>
#include <vector>

namespace Entelechy {

class AgentBridge {
public:
    World m_world;
    Scheduler m_scheduler;
    MovementSystem m_movement_system;

    void init();
    void step(float dt);

    // Structured tool interfaces (Milestone 0.4)
    std::string queryEntities(const std::string& comp_name) const;
    std::string getComponent(Entity e, const std::string& comp_name) const;
    std::string setComponent(Entity e, const std::string& comp_name, const std::string& json);

    // ToolRegistry integration
    std::string callTool(const std::string& name, const std::string& json_args) const;
};

} // namespace Entelechy
