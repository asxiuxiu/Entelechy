#pragma once
#include "render_system/render_world/extract_schedule.h"

namespace Entelechy
{

// ExtractSkySystem — copies the first main-world SkySettings into an
// ExtractedSky in the render world. No SkySettings in the main world means
// no ExtractedSky; the execute stage then skips the sky pass entirely and
// the plain clear color shows through.
class ExtractSkySystem : public IExtractSystem
{
public:
    void extract(const World &mainWorld, World &renderWorld, FrameArena &arena, f32 dt) override;
};

} // namespace Entelechy
