#pragma once
#include "core/foundation_types.h"
#include "ecs/type/entity_registry.h"
#include <memory>

namespace Entelechy
{

class GLRHIDevice;
class IRHIDevice;
class ShaderCache;
class World;
class IRHICommandList;
class PrepareAssetsSystem;
struct ExtractedView;
struct PreparedMesh;
struct PreparedMaterial;

// ExecuteStats — per-frame counters of the Execute stage.
struct ExecuteStats
{
    u32 draw_calls = 0;
    u32 skipped_missing_mesh = 0;     // entity lacks RenderTransform/RenderMesh
    u32 skipped_missing_material = 0; // (unused, kept for panel compatibility)
    u32 fallback_mesh_draws = 0;      // drawn with the fallback cube
    u32 fallback_material_draws = 0;  // drawn with the pink fallback material
};

// RenderExecuteSystem — consumes ViewBinnedPhases/ViewSortedPhases from the
// view entity and issues GPU draw calls through the RHI. Final stage of the
// render pipeline (Extract -> Prepare -> Cull -> Queue -> Execute).
//
// GPU resources are NOT owned here: the Prepare stage resolves asset handles
// to PreparedMesh/PreparedMaterial; this system only looks them up (falling
// back to the placeholder cube/pink material while assets stream in).
//
// Remaining simplifications (tracked in TODO.md):
// - Owns a second GLRHIDevice + ShaderCache, same pattern as SimpleCubeRenderer.
class RenderExecuteSystem
{
public:
    RenderExecuteSystem();
    ~RenderExecuteSystem();

    RenderExecuteSystem(const RenderExecuteSystem &) = delete;
    RenderExecuteSystem &operator=(const RenderExecuteSystem &) = delete;

    bool init();
    void shutdown();

    // Device/shader cache access for the Prepare stage (borrowed, not owned).
    IRHIDevice *device();
    ShaderCache *shaderCache();

    // Draws everything queued for the first view entity. No-op without a view.
    void run(World &renderWorld, PrepareAssetsSystem &prepare);

    // Frame fence + deferred-delete flush. Call once per frame after present.
    void endFrame();

    [[nodiscard]] const ExecuteStats &stats() const
    {
        return m_stats;
    }

private:
    void drawItem(World &renderWorld, const ExtractedView &view, Entity renderEntity, IRHICommandList *cmdList,
                  PrepareAssetsSystem &prepare);

    std::unique_ptr<GLRHIDevice> m_device;
    std::unique_ptr<ShaderCache> m_shader_cache;
    ExecuteStats m_stats;
    bool m_initialized = false;
};

} // namespace Entelechy
