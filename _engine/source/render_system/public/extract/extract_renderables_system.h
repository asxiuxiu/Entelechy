#pragma once
#include "render_system/render_world/extract_schedule.h"
#include "render_system/extract/main_world_sync.h"

namespace Entelechy
{

// ExtractRenderablesSystem — copies (MeshAssetRef, MaterialAssetRef, GlobalTransform)
// from the main world into the render world as (RenderMesh, RenderMaterial, RenderTransform).
class ExtractRenderablesSystem : public IExtractSystem
{
public:
    explicit ExtractRenderablesSystem(MainWorldSync &sync) : m_sync(sync) {}

    void extract(const World &mainWorld, World &renderWorld, FrameArena &arena, f32 dt) override;

private:
    MainWorldSync &m_sync;
};

} // namespace Entelechy
