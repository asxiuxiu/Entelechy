#pragma once
#include "world.h"
#include "scheduler.h"
#include "movement_system.h"
#include "tool_registry.h"
#include "base/small_string.h"

namespace Entelechy {

class AgentBridge {
public:
    World m_world;
    Scheduler m_scheduler;
    MovementSystem m_movement_system;

    void init();
    void step(f32 dt);

    // Structured tool interfaces (Milestone 0.4)
    SmallString queryEntities(const SmallString& comp_name) const;
    SmallString getComponent(Entity e, const SmallString& comp_name) const;
    SmallString setComponent(Entity e, const SmallString& comp_name, const SmallString& json);

    // New AI-friendly tools
    SmallString queryEntitiesByMask(u64 mask) const;
    SmallString getWorldSummary() const;

    // ToolRegistry integration
    SmallString callTool(const SmallString& name, const SmallString& json_args) const;
};

} // namespace Entelechy
