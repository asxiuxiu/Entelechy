#pragma once
#include "asset/handle/asset_handle.h"
#include "asset/type/assets.h"
#include "asset/type/material_asset.h"
#include "asset/type/mesh_asset.h"

namespace game
{

// RenderAssets — demo asset storage shared between GamePlugin (component
// refs in the main world) and main.cpp (manual GPU resource registration
// on RenderExecuteSystem).
//
// Phase 2a: the inserted assets are empty placeholders; only the minted
// handles matter. The Prepare stage replaces the manual GPU registration
// in Phase 2c.
struct RenderAssets
{
    Entelechy::Assets<Entelechy::MeshAsset> mesh_assets;
    Entelechy::Assets<Entelechy::MaterialAsset> material_assets;

    Entelechy::Handle<Entelechy::MeshAsset> cube_mesh;
    Entelechy::Handle<Entelechy::MaterialAsset> mat_red;
    Entelechy::Handle<Entelechy::MaterialAsset> mat_green;
    Entelechy::Handle<Entelechy::MaterialAsset> mat_blue;
    Entelechy::Handle<Entelechy::MaterialAsset> mat_yellow;
};

// Process-wide demo asset storage (function-local static, initialized on
// first use).
inline RenderAssets &renderAssets()
{
    static RenderAssets instance;
    return instance;
}

// Inserts the placeholder assets and caches their handles. Idempotent, so
// both GamePlugin::setup() and main() can call it safely.
inline void initRenderAssets()
{
    RenderAssets &assets = renderAssets();
    if (assets.cube_mesh.valid())
        return;

    assets.cube_mesh = assets.mesh_assets.insert(Entelechy::MeshAsset{});
    assets.mat_red = assets.material_assets.insert(Entelechy::MaterialAsset{});
    assets.mat_green = assets.material_assets.insert(Entelechy::MaterialAsset{});
    assets.mat_blue = assets.material_assets.insert(Entelechy::MaterialAsset{});
    assets.mat_yellow = assets.material_assets.insert(Entelechy::MaterialAsset{});
}

} // namespace game
