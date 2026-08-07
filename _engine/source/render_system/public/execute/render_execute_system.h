#pragma once
#include "core/foundation_types.h"
#include "ecs/type/entity_registry.h"
#include "render/material/material.h"
#include "render/rhi/rhi_resources.h"
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
struct ExtractedLight;
struct ExtractedSky;
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
class RenderExecuteSystem
{
public:
    RenderExecuteSystem();
    ~RenderExecuteSystem();

    RenderExecuteSystem(const RenderExecuteSystem &) = delete;
    RenderExecuteSystem &operator=(const RenderExecuteSystem &) = delete;

    // Initialize with an externally-owned RHI device. The device must
    // outlive this system. Pass nullptr to skip initialization.
    bool init(IRHIDevice *device);
    void shutdown();

    // Device/shader cache access for the Prepare stage (borrowed, not owned).
    IRHIDevice *device();
    ShaderCache *shaderCache();

    // PSO cache size of the owned device (for the stats panel).
    usize psoCacheSize() const;

    // Draws everything queued for the first view entity. No-op without a view.
    void run(World &renderWorld, PrepareAssetsSystem &prepare);

    // Frame fence + deferred-delete flush. Call once per frame after present.
    void endFrame();

    [[nodiscard]] const ExecuteStats &stats() const
    {
        return m_stats;
    }

private:
    void drawItem(World &renderWorld, const ExtractedView &view, const ExtractedLight &light, Entity renderEntity,
                  IRHICommandList *cmdList, PrepareAssetsSystem &prepare);

    // Sky gradient pass. initSkyPass is best-effort: on
    // failure the pass stays disabled and the plain clear color shows.
    bool initSkyPass();
    void drawSky(const ExtractedView &view, const ExtractedSky &sky, IRHICommandList *cmdList);

    std::unique_ptr<ShaderCache> m_shader_cache;
    IRHIDevice *m_device = nullptr; // Borrowed, not owned
    Material m_sky_material;
    RHIBufferRef m_sky_vbo;
    bool m_sky_ready = false;
    ExecuteStats m_stats;
    bool m_initialized = false;
};

} // namespace Entelechy
