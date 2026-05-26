#pragma once
#include "world.h"
#include "render_world/ExtractSchedule.h"
#include "extract/MainWorldSync.h"

namespace Entelechy {

// RenderWorld — self-contained ECS world + extract schedule for rendering.
// Checkpoint 1: holds the extracted copy of main-world renderables and camera.
class RenderWorld {
public:
    RenderWorld();

    World& world() { return m_world; }
    const World& world() const { return m_world; }

    ExtractSchedule& extractSchedule() { return m_extract_schedule; }

    MainWorldSync& sync() { return m_sync; }
    const MainWorldSync& sync() const { return m_sync; }

    // Clears previous frame state, then runs the extract schedule.
    void extract(const World& mainWorld, f32 dt);

    // Destroys all entities and clears sync mappings.
    void clear();

private:
    World m_world;
    ExtractSchedule m_extract_schedule;
    MainWorldSync m_sync;
};

} // namespace Entelechy
