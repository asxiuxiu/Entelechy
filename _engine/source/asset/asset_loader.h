#pragma once
#include "vfs_types.h"
#include "path.h"

namespace Entelechy {

// ------------------------------------------------------------------
// IAssetLoader<T> — per-type deserialization interface
// ------------------------------------------------------------------
// Implement this for each asset type (Mesh, Texture, Shader, ...).
// The AssetServer will call load() from the IO thread.
//
// Example:
//   class MeshLoader : public IAssetLoader<Mesh> {
//   public:
//       Mesh load(const FileData& data, const Path& path) override {
//           // parse bytes -> Mesh
//       }
//   };
// ------------------------------------------------------------------
template<typename T>
class IAssetLoader {
public:
    virtual ~IAssetLoader() = default;
    virtual T load(const FileData& data, const Path& path) = 0;
};

} // namespace Entelechy
