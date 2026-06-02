#pragma once
#include "vfs/vfs_types.h"
#include "core/string/string.h"
#include "core/string/string_id.h"
#include "core/container/hash_map.h"
#include "core/container/dynamic_array.h"

namespace Entelechy {

class IMountPoint {
public:
    virtual ~IMountPoint() = default;
    virtual bool exists(const char* path) = 0;
    virtual FileData readFile(const char* path) = 0;
    virtual bool canWrite() const { return false; }
    virtual bool writeFile(const char* path, const u8* data, usize size) { (void)path; (void)data; (void)size; return false; }
};

class FileSystemMountPoint : public IMountPoint {
public:
    explicit FileSystemMountPoint(const char* root);
    bool exists(const char* path) override;
    FileData readFile(const char* path) override;
    bool canWrite() const override { return true; }
    bool writeFile(const char* path, const u8* data, usize size) override;

private:
    String m_root;
    void buildFullPath(const char* path, char* out, usize outSize) const;
};

class MemoryMountPoint : public IMountPoint {
public:
    void registerFile(const char* path, const u8* data, usize size);
    bool exists(const char* path) override;
    FileData readFile(const char* path) override;
    bool canWrite() const override { return true; }
    bool writeFile(const char* path, const u8* data, usize size) override;

private:
    HashMap<StringId, DynamicArray<u8>> m_files;
};

} // namespace Entelechy
