#pragma once
#include "core/foundation_types.h"
#include "core/string/small_string.h"
#include "core/path/path.h"

namespace Entelechy {

class World;

// ------------------------------------------------------------------
// SceneSerializer -- save / load World state via reflection
// ------------------------------------------------------------------
// Uses TypeRegistry + AtomRegistry to recursively serialize component
// fields. No hand-written per-component code is required.
// ------------------------------------------------------------------
class SceneSerializer {
public:
    // Serialize the entire world to a JSON string.
    SmallString serialize(const World& world) const;

    // Deserialize JSON into the given world. Clears existing entities first.
    bool deserialize(World& world, const SmallString& json) const;

    // Convenience: write to file (uses fopen; VFS wrapper can be added later).
    bool saveToFile(const World& world, const Path& path) const;

    // Convenience: read from file.
    bool loadFromFile(World& world, const Path& path) const;
};

} // namespace Entelechy
