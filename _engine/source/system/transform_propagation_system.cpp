#include "transform_propagation_system.h"
#include "query.h"
#include "transform_component.h"
#include "mat4.h"

namespace Entelechy {

void TransformPropagationSystem::tick(World& world, FrameArena& arena, float dt) {
    (void)arena;
    (void)dt;
    for (auto [e, trans, global] : Query<Transform, GlobalTransform>(world)) {
        if (!trans || !global) continue;
        if (trans->dirty) {
            global->matrix = Mat4::fromTRS(trans->translation, trans->rotation, trans->scale);
            trans->dirty = 0;
        }
    }
}

} // namespace Entelechy
