#pragma once
#include "ecs/world/world.h"
#include "ecs/world/scheduler.h"
#include "motor/movement_system.h"
#include "bridge/tool_registry.h"
#include "core/string/string.h"

namespace Entelechy
{

class AgentBridge
{
public:
    World m_world;
    Scheduler m_scheduler;
    MovementSystem m_movement_system;

    void init();
    void step(f32 dt);

    // Structured tool interfaces (Milestone 0.4)
    String queryEntities(const String &comp_name) const;
    String getComponent(Entity e, const String &comp_name) const;
    String setComponent(Entity e, const String &comp_name, const String &json);

    // New AI-friendly tools
    String queryEntitiesByMask(u64 mask) const;
    String getWorldSummary() const;

    // ToolRegistry integration
    String callTool(const String &name, const String &json_args) const;
};

} // namespace Entelechy
