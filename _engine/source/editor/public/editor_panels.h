#pragma once
#include "core/foundation_types.h"

namespace Entelechy
{

// Forward declarations — EditorLib is the bridge between ImGui UI and ECS runtime.
class World;
class Scheduler;

// ECS World + component Inspector (two-column: Entity list + Component detail).
// autoRun is an in/out parameter controlling Scheduler tick.
void buildECSInspector(World &world, Scheduler &scheduler, f32 dt, bool &autoRun);

} // namespace Entelechy
