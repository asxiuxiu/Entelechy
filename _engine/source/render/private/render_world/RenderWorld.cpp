#include "render/render_world/RenderWorld.h"
#include "core/memory/frame_arena.h"

namespace Entelechy {

RenderWorld::RenderWorld() = default;

void RenderWorld::extract(const World& mainWorld, f32 dt) {
    clear();

    FrameArena arena(1024 * 1024); // 1 MiB temporary scratch for extract
    m_extract_schedule.run(mainWorld, m_world, arena, dt);
}

void RenderWorld::clear() {
    // Destroy every alive entity in the render world.
    usize maxId = m_world.maxEntityID();
    for (u32 id = 0; id < maxId; ++id) {
        Entity e{id, m_world.getEntityGeneration(id)};
        if (m_world.valid(e)) {
            m_world.destroyImmediate(e);
        }
    }
    m_sync.clear();
}

} // namespace Entelechy
