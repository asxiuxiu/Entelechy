#pragma once
#include "asset/handle/asset_handle.h"
#include "asset/type/assets.h"
#include "asset/type/material_asset.h"
#include "asset/type/mesh_asset.h"
#include "asset/type/texture_asset.h"
#include "core/container/dynamic_array.h"
#include "core/container/hash_map.h"
#include "core/foundation_types.h"
#include "render/material/material.h"
#include "render/rhi/rhi_resources.h"

namespace Entelechy
{

class IRHIDevice;
class ShaderCache;
class World;

// ------------------------------------------------------------------
// Prepared resources — GPU-side counterparts of CPU assets
// ------------------------------------------------------------------
struct PreparedMesh
{
    RHIBufferRef vbo;
    RHIBufferRef ibo;
    u32 index_count = 0;
};

struct PreparedMaterial
{
    Material material;
};

// PrepareStats — per-frame counters of the Prepare stage.
struct PrepareStats
{
    u32 prepared_meshes = 0;
    u32 prepared_materials = 0;
    u32 pending_meshes = 0;     // asset not loaded yet -> fallback mesh in use
    u32 pending_materials = 0;  // asset or its texture not loaded yet -> pink fallback
};

// ------------------------------------------------------------------
// PrepareAssetsSystem — resolves extracted asset handles to GPU resources
// ------------------------------------------------------------------
// Sits between Extract and Cull in the frame pipeline:
//   Extract -> Prepare -> Cull -> Queue -> Execute
// Scans the render world for RenderMesh/RenderMaterial handles each
// frame and resolves them through the game-side Assets<T> storages:
//   - loaded   -> create (once) and cache the GPU resource
//   - pending  -> keep the entity on the fallback (unit cube / pink
//                 material); the real resource replaces it automatically
//                 once the async load lands
// Textures referenced by a MaterialAsset are prepared on demand; a
// material whose texture is still loading stays pending as a whole.
// ------------------------------------------------------------------
class PrepareAssetsSystem
{
public:
    PrepareAssetsSystem() = default;
    ~PrepareAssetsSystem();

    PrepareAssetsSystem(const PrepareAssetsSystem &) = delete;
    PrepareAssetsSystem &operator=(const PrepareAssetsSystem &) = delete;

    // Creates the fallback resources (unit cube mesh, 1x1 white texture,
    // pink fallback material). Device/shader cache are borrowed from the
    // execute stage and must outlive this system.
    bool init(IRHIDevice *device, ShaderCache *shaderCache);
    void shutdown();

    // Injection point for the asset storages (owned by the game side).
    void bindAssets(Assets<MeshAsset> *meshes, Assets<MaterialAsset> *materials, Assets<TextureAsset> *textures);

    // Scans the render world and resolves handles to GPU resources.
    void run(World &renderWorld);

    // Returns nullptr when the handle is not prepared yet (caller falls
    // back). Non-const: the execute stage mutates per-draw uniforms.
    PreparedMesh *findMesh(Handle<MeshAsset> handle);
    PreparedMaterial *findMaterial(Handle<MaterialAsset> handle);

    PreparedMesh *fallbackMesh()
    {
        return &m_fallback_mesh;
    }
    PreparedMaterial *fallbackMaterial()
    {
        return &m_fallback_material;
    }

    [[nodiscard]] const PrepareStats &stats() const
    {
        return m_stats;
    }

private:
    bool uploadMesh(const MeshAsset &asset, PreparedMesh &out);
    bool prepareMesh(Handle<MeshAsset> handle);
    bool prepareMaterial(Handle<MaterialAsset> handle);
    RHITextureRef prepareTexture(Handle<TextureAsset> handle);

    IRHIDevice *m_device = nullptr;
    ShaderCache *m_shader_cache = nullptr;

    Assets<MeshAsset> *m_mesh_assets = nullptr;
    Assets<MaterialAsset> *m_material_assets = nullptr;
    Assets<TextureAsset> *m_texture_assets = nullptr;

    HashMap<Handle<MeshAsset>, PreparedMesh> m_meshes;
    HashMap<Handle<MaterialAsset>, PreparedMaterial> m_materials;
    HashMap<Handle<TextureAsset>, RHITextureRef> m_textures;

    // Handles seen while still pending (one-shot "waiting" logs).
    DynamicArray<Handle<MeshAsset>> m_pending_meshes_logged;
    DynamicArray<Handle<MaterialAsset>> m_pending_logged;

    PreparedMesh m_fallback_mesh;
    PreparedMaterial m_fallback_material;
    RHITextureRef m_white_texture;

    PrepareStats m_stats;
    bool m_initialized = false;
};

} // namespace Entelechy
