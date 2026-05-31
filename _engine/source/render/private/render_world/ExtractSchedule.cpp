#include "render/render_world/ExtractSchedule.h"

namespace Entelechy {

void ExtractSchedule::registerSystem(IExtractSystem* system) {
    m_systems.pushBack(system);
}

void ExtractSchedule::run(const World& mainWorld, World& renderWorld, FrameArena& arena, f32 dt) {
    for (auto* sys : m_systems) {
        sys->extract(mainWorld, renderWorld, arena, dt);
    }
}

void ExtractSchedule::clear() {
    m_systems.clear();
}

} // namespace Entelechy
