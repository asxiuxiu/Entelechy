#include "vfs/mount_point.h"
#include <cstdio>
#include <cstring>

#if PLATFORM_WINDOWS
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace Entelechy {

static bool ensureDirExists(const char* path) {
    char buf[512];
    usize len = std::strlen(path);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    std::memcpy(buf, path, len);
    buf[len] = '\0';

    // Trim to last separator.
    char* sep = nullptr;
    for (char* p = buf; *p; ++p) {
        if (*p == '/' || *p == '\\') sep = p;
    }
    if (!sep) return true; // no directory component
    *sep = '\0';

    if (buf[0] == '\0') return true;

    // Recursively create parent directories (e.g. a/b/c when a or a/b don't exist).
    for (char* p = buf; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            char save = *p;
            *p = '\0';
#if PLATFORM_WINDOWS
            _mkdir(buf);
#else
            mkdir(buf, 0755);
#endif
            *p = save;
        }
    }

    // Create the final directory itself.
#if PLATFORM_WINDOWS
    _mkdir(buf);
#else
    mkdir(buf, 0755);
#endif
    return true;
}

// ---------- FileSystemMountPoint ----------

FileSystemMountPoint::FileSystemMountPoint(const char* root) {
    m_root = root;
}

void FileSystemMountPoint::buildFullPath(const char* path, char* out, usize outSize) const {
    const char* root = m_root.c_str();
    usize rootLen = m_root.length();

    usize i = 0;
    for (; i < rootLen && i < outSize - 1; ++i) {
        out[i] = root[i];
    }

    if (i > 0 && out[i - 1] != '/' && out[i - 1] != '\\' && i < outSize - 1) {
        out[i++] = '/';
    }

    const char* p = path;
    if (*p == '/' || *p == '\\') ++p;

    for (; *p && i < outSize - 1; ++p) {
        out[i++] = (*p == '\\') ? '/' : *p;
    }

    out[i] = '\0';
}

bool FileSystemMountPoint::exists(const char* path) {
    char fullPath[512];
    buildFullPath(path, fullPath, sizeof(fullPath));

    FILE* f = std::fopen(fullPath, "rb");
    if (f) {
        std::fclose(f);
        return true;
    }
    return false;
}

FileData FileSystemMountPoint::readFile(const char* path) {
    char fullPath[512];
    buildFullPath(path, fullPath, sizeof(fullPath));

    FILE* f = std::fopen(fullPath, "rb");
    if (!f) {
        return FileData{};
    }

    std::fseek(f, 0, SEEK_END);
    long fileSize = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    FileData result;
    if (fileSize > 0) {
        result.bytes.resize(static_cast<usize>(fileSize));
        std::fread(result.bytes.data(), 1, static_cast<usize>(fileSize), f);
    }
    std::fclose(f);

    result.valid = true;
    return result;
}

bool FileSystemMountPoint::writeFile(const char* path, const u8* data, usize size) {
    char fullPath[512];
    buildFullPath(path, fullPath, sizeof(fullPath));

    ensureDirExists(fullPath);

    FILE* f = std::fopen(fullPath, "wb");
    if (!f) {
        return false;
    }

    if (size > 0) {
        std::fwrite(data, 1, size, f);
    }
    std::fclose(f);
    return true;
}

// ---------- MemoryMountPoint ----------

void MemoryMountPoint::registerFile(const char* path, const u8* data, usize size) {
    DynamicArray<u8> copy;
    if (size > 0) {
        copy.resize(size);
        for (usize i = 0; i < size; ++i) {
            copy[i] = data[i];
        }
    }
    m_files.insert(SmallString(path), std::move(copy));
}

bool MemoryMountPoint::exists(const char* path) {
    return m_files.find(SmallString(path)) != nullptr;
}

FileData MemoryMountPoint::readFile(const char* path) {
    auto* entry = m_files.find(SmallString(path));
    if (!entry) {
        return FileData{};
    }

    FileData result;
    result.bytes = *entry;
    result.valid = true;
    return result;
}

bool MemoryMountPoint::writeFile(const char* path, const u8* data, usize size) {
    DynamicArray<u8> copy;
    if (size > 0) {
        copy.resize(size);
        for (usize i = 0; i < size; ++i) {
            copy[i] = data[i];
        }
    }
    m_files.insert(SmallString(path), std::move(copy));
    return true;
}

} // namespace Entelechy
