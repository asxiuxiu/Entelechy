#pragma once
#include "render/render_world/ExtractSchedule.h"
#include "render/extract/MainWorldSync.h"

namespace Entelechy {

// ExtractRenderablesSystem — copies (MeshAssetRef, MaterialAssetRef, GlobalTransform)
// from the main world into the render world as (RenderMesh, RenderMaterial, RenderTransform).
class ExtractRenderablesSystem : public IExtractSystem {
public:
    explicit ExtractRenderablesSystem(MainWorldSync& sync) : m_sync(sync) {}

    void extract(const World& mainWorld, World& renderWorld, FrameArena& arena, f32 dt) override;

private:
    MainWorldSync& m_sync;
};

} // namespace Entelechy
