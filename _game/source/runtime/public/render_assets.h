#pragma once
#include "asset/handle/asset_handle.h"
#include "asset/loader/asset_server.h"
#include "asset/loader/mesh_asset_loader.h"
#include "asset/loader/texture_asset_loader.h"
#include "asset/type/assets.h"
#include "asset/type/material_asset.h"
#include "asset/type/mesh_asset.h"
#include "asset/type/mesh_primitives.h"
#include "asset/type/texture_asset.h"
#include "core/allocator/allocator.h"
#include "log/core/log_macros.h"
#include "vfs/mount_point.h"
#include "vfs/vfs.h"
#include <memory>

namespace game
{

// RenderAssets — demo asset storage and loading infrastructure shared
// between GamePlugin (component refs in the main world) and main.cpp
// (binding the storages to the render Prepare stage).
//
// Phase 2c: owns the VFS + AssetServer + loader used to stream the demo
// texture asynchronously. Procedural meshes/materials are inserted
// directly; the checker texture arrives via loadAsync, so the ground
// starts on the pink fallback and flips to the checker once loaded.
struct RenderAssets
{
    // NOTE: no FileSystemMountPoint members here — VFS takes ownership of
    // mounted backends and releases them with DefaultAllocator::free, so
    // mount points must be DefaultAllocator-allocated (see initRenderAssets).
    Entelechy::VFS vfs;
    Entelechy::AssetServer asset_server{&vfs};
    Entelechy::TextureAssetLoader texture_loader;
    Entelechy::MeshAssetLoader mesh_loader;

    Entelechy::Assets<Entelechy::MeshAsset> mesh_assets;
    Entelechy::Assets<Entelechy::MaterialAsset> material_assets;
    Entelechy::Assets<Entelechy::TextureAsset> texture_assets;

    Entelechy::Handle<Entelechy::MeshAsset> cube_mesh;
    Entelechy::Handle<Entelechy::MeshAsset> ground_mesh;
    Entelechy::Handle<Entelechy::MaterialAsset> mat_red;
    Entelechy::Handle<Entelechy::MaterialAsset> mat_green;
    Entelechy::Handle<Entelechy::MaterialAsset> mat_blue;
    Entelechy::Handle<Entelechy::MaterialAsset> mat_yellow;
    Entelechy::Handle<Entelechy::MaterialAsset> mat_checker;
    Entelechy::Handle<Entelechy::MaterialAsset> mat_white;
    Entelechy::Handle<Entelechy::TextureAsset> checker_texture;
};

// Process-wide demo asset storage (function-local static, initialized on
// first use).
inline RenderAssets &renderAssets()
{
    static RenderAssets instance;
    return instance;
}

// Inserts the procedural assets, kicks off the async texture load and
// caches all handles. Idempotent, so both GamePlugin::setup() and main()
// can call it safely.
inline void initRenderAssets()
{
    using namespace Entelechy;
    RenderAssets &assets = renderAssets();
    if (assets.cube_mesh.valid())
        return;

    // Three roots for the same content dir: the exe is run from the
    // project root (CLI), from build/bin/Debug (VS debugger default
    // working directory) and by double-clicking build/bin/Debug/*.exe
    // (cwd = exe dir); VFS tries every mount until one resolves.
    // VFS owns the backends and frees them via DefaultAllocator::free, so
    // allocate the mount points with DefaultAllocator + construct_at.
    auto *contentMount = static_cast<Entelechy::FileSystemMountPoint *>(
        Entelechy::DefaultAllocator::alloc(sizeof(Entelechy::FileSystemMountPoint),
                                           alignof(Entelechy::FileSystemMountPoint)));
    std::construct_at(contentMount, "_content");
    assets.vfs.mount("content", contentMount);

    auto *contentMountVs = static_cast<Entelechy::FileSystemMountPoint *>(
        Entelechy::DefaultAllocator::alloc(sizeof(Entelechy::FileSystemMountPoint),
                                           alignof(Entelechy::FileSystemMountPoint)));
    std::construct_at(contentMountVs, "../../_content");
    assets.vfs.mount("content_vs", contentMountVs);

    auto *contentMountBin = static_cast<Entelechy::FileSystemMountPoint *>(
        Entelechy::DefaultAllocator::alloc(sizeof(Entelechy::FileSystemMountPoint),
                                           alignof(Entelechy::FileSystemMountPoint)));
    std::construct_at(contentMountBin, "../../../_content");
    assets.vfs.mount("content_bin", contentMountBin);

    // Procedural meshes (available immediately).
    assets.cube_mesh = assets.mesh_assets.insert(buildCubeMesh(0.5f));
    assets.ground_mesh = assets.mesh_assets.insert(buildGroundMesh(20.0f, 20.0f));

    // Solid-color materials.
    assets.mat_red = assets.material_assets.insert(MaterialAsset{{0.85f, 0.20f, 0.20f}, {}});
    assets.mat_green = assets.material_assets.insert(MaterialAsset{{0.20f, 0.85f, 0.25f}, {}});
    assets.mat_blue = assets.material_assets.insert(MaterialAsset{{0.25f, 0.35f, 0.90f}, {}});
    assets.mat_yellow = assets.material_assets.insert(MaterialAsset{{0.90f, 0.80f, 0.20f}, {}});

    // Textured material: the checker PNG streams in asynchronously; the
    // Prepare stage keeps the ground on the pink fallback until it lands.
    assets.checker_texture =
        assets.asset_server.loadAsync(Path{"demo/checker.png"}, assets.texture_loader, assets.texture_assets);
    assets.mat_checker = assets.material_assets.insert(MaterialAsset{{1.0f, 1.0f, 1.0f}, assets.checker_texture});
    LOG_INFO(LogCategories::kEngine, "RenderAssets: requested async load of demo/checker.png");

    // Shared white-model material for the cooked Sponza scene (Phase 3c):
    // grey albedo, no texture, normal shading path.
    assets.mat_white = assets.material_assets.insert(MaterialAsset{{0.75f, 0.75f, 0.75f}, {}, 1.0f});
}

} // namespace game
