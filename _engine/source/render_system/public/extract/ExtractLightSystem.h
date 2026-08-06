#pragma once
#include "render_system/render_world/ExtractSchedule.h"

namespace Entelechy
{

// ExtractLightSystem — copies the first main-world DirectionalLight into an
// ExtractedLight in the render world. No light in the main world means no
// ExtractedLight; the execute stage falls back to ambient-only defaults.
class ExtractLightSystem : public IExtractSystem
{
public:
    void extract(const World &mainWorld, World &renderWorld, FrameArena &arena, f32 dt) override;
};

} // namespace Entelechy
