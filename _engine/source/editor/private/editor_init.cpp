#include "editor/editor_init.h"
#include "ecs/type/atom_registry.h"
#include "log/core/log_macros.h"

namespace Entelechy
{

void initEditor()
{
    // Register ImGui draw callbacks for builtin atom types so the Inspector
    // can render f32, bool, i32, u32, StringId, String fields.
    // Idempotent: registerAtom() keys by StringId, so repeated calls are safe.
    AtomRegistry::instance().registerBuiltinAtoms();
    LOG_INFO(LogCategories::kEngine, "Editor module registered (ECS inspector + atom draw registration)");
}

} // namespace Entelechy
