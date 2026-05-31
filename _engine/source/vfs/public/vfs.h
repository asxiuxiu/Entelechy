#pragma once
#include "vfs/mount_point.h"
#include "core/container/dynamic_array.h"
#include "core/path/path.h"
#include "core/string/string_id.h"

namespace Entelechy {

class VFS {
public:
    ~VFS();

    void mount(const char* logicalPath, IMountPoint* backend);
    void unmount(const char* logicalPath);

    [[nodiscard]] bool exists(const Path& path) const;
    [[nodiscard]] FileData readFile(const Path& path) const;
    bool writeFile(const Path& path, const u8* data, usize size);

    void clear();

private:
    struct MountEntry {
        StringId name;
        IMountPoint* backend;
    };
    DynamicArray<MountEntry> m_mounts;
};

} // namespace Entelechy
