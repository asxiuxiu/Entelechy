#pragma once
#include "core/foundation_types.h"
#include "core/container/hash_map.h"
#include "core/math/vec.h"
#include "asset/handle/asset_handle.h"
#include "asset/type/material_asset.h"
#include "asset/type/mesh_asset.h"
#include "ecs/type/entity_registry.h"
#include "render/rhi/rhi_types.h"
#include "render/rhi/rhi_resources.h"
#include "render/material/material.h"
#include <memory>

namespace Entelechy
{

class GLRHIDevice;
class ShaderCache;
class World;
class IRHICommandList;
struct ExtractedView;

// ExecuteStats — per-frame counters of the Execute stage.
struct ExecuteStats
{
    u32 draw_calls = 0;
    u32 skipped_missing_mesh = 0;
    u32 skipped_missing_material = 0;
};

// RenderExecuteSystem — consumes ViewBinnedPhases/ViewSortedPhases from the
// view entity and issues GPU draw calls through the RHI. Final stage of the
// Phase-1 render pipeline (Extract -> Cull -> Queue -> Execute).
//
// Phase 1 simplifications (tracked in TODO.md):
// - Owns a second GLRHIDevice + ShaderCache, same pattern as SimpleCubeRenderer.
// - Mesh/material GPU resources are registered manually by the caller instead
//   of being resolved by a Prepare stage.
class RenderExecuteSystem
{
public:
    RenderExecuteSystem();
    ~RenderExecuteSystem();

    RenderExecuteSystem(const RenderExecuteSystem &) = delete;
    RenderExecuteSystem &operator=(const RenderExecuteSystem &) = delete;

    bool init();
    void shutdown();

    // Registers GPU geometry for a mesh asset handle. Returns false on failure.
    bool registerMesh(Handle<MeshAsset> handle, const void *vertexData, usize vertexBytes, u32 vertexStride,
                      const VertexAttributeDesc *attrs, u32 attrCount, const u32 *indexData, u32 indexCount);

    // Registers an unlit solid-color material (uMVP + uColor) for a material
    // asset handle. The color is baked into the material at registration time.
    bool registerColorMaterial(Handle<MaterialAsset> handle, const Vec3 &color);

    // Draws everything queued for the first view entity. No-op without a view.
    void run(World &renderWorld);

    // Frame fence + deferred-delete flush. Call once per frame after present.
    void endFrame();

    [[nodiscard]] const ExecuteStats &stats() const
    {
        return m_stats;
    }

private:
    struct GpuMesh
    {
        RHIBufferRef vbo;
        RHIBufferRef ibo;
        u32 index_count = 0;
    };

    void drawItem(World &renderWorld, const ExtractedView &view, Entity renderEntity, IRHICommandList *cmdList);

    std::unique_ptr<GLRHIDevice> m_device;
    std::unique_ptr<ShaderCache> m_shader_cache;
    HashMap<Handle<MeshAsset>, GpuMesh> m_meshes;
    HashMap<Handle<MaterialAsset>, Material> m_materials;
    ExecuteStats m_stats;
    bool m_initialized = false;
};

} // namespace Entelechy
