#include "vfs.h"
#include "string_intern_pool.h"

namespace Entelechy {

VFS::~VFS() {
    clear();
}

void VFS::mount(const char* logicalPath, IMountPoint* backend) {
    if (!backend) return;
    MountEntry entry;
    entry.name = StringId(logicalPath);
    entry.backend = backend;
    m_mounts.pushBack(entry);
}

void VFS::unmount(const char* logicalPath) {
    StringId target(logicalPath);
    for (usize i = 0; i < m_mounts.size(); ++i) {
        if (m_mounts[i].name == target) {
            delete m_mounts[i].backend;
            m_mounts.removeAt(i);
            return;
        }
    }
}

bool VFS::exists(const Path& path) const {
    // Later mounts take priority: search from back to front
    for (usize i = m_mounts.size(); i > 0; --i) {
        if (m_mounts[i - 1].backend->exists(path.c_str())) {
            return true;
        }
    }
    return false;
}

FileData VFS::readFile(const Path& path) const {
    // Later mounts take priority: search from back to front
    for (usize i = m_mounts.size(); i > 0; --i) {
        IMountPoint* mp = m_mounts[i - 1].backend;
        if (mp->exists(path.c_str())) {
            return mp->readFile(path.c_str());
        }
    }
    return FileData{};
}

bool VFS::writeFile(const Path& path, const u8* data, usize size) {
    // Later mounts take priority for writes too
    for (usize i = m_mounts.size(); i > 0; --i) {
        IMountPoint* mp = m_mounts[i - 1].backend;
        if (mp->canWrite() && mp->exists(path.c_str())) {
            return mp->writeFile(path.c_str(), data, size);
        }
    }
    return false;
}

void VFS::clear() {
    for (usize i = 0; i < m_mounts.size(); ++i) {
        delete m_mounts[i].backend;
    }
    m_mounts.clear();
}

} // namespace Entelechy
