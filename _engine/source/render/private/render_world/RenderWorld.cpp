#include "render/render_world/RenderWorld.h"

namespace Entelechy
{

RenderWorld::RenderWorld() = default;

void RenderWorld::extract(const World &mainWorld, f32 dt)
{
    clear();

    FrameArena &arena = m_arena_ring.current();
    m_extract_schedule.run(mainWorld, m_world, arena, dt);
    m_arena_ring.advance();
}

void RenderWorld::clear()
{
    m_world.clearAllEntities();
    m_sync.clear();
}

} // namespace Entelechy
