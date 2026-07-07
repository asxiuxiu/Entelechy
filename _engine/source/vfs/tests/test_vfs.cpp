#include "test/test_framework.h"
#include "vfs/vfs.h"
#include "vfs/mount_point.h"
#include "core/allocator/allocator.h"
#include <memory>

TEST(VFS, FileSystemMountPointReadWrite)
{
    Entelechy::VFS vfs;
    void *fsMem = Entelechy::DefaultAllocator::alloc(sizeof(Entelechy::FileSystemMountPoint),
                                                     alignof(Entelechy::FileSystemMountPoint));
    auto *fsMount = std::construct_at(static_cast<Entelechy::FileSystemMountPoint *>(fsMem), "build/test_vfs");
    vfs.mount("/test", fsMount);

    const char *testData = "Hello from VFS!";
    usize testLen = 17;
    bool writeOk = fsMount->writeFile("hello.txt", reinterpret_cast<const u8 *>(testData), testLen);
    ASSERT_TRUE(writeOk);

    auto file = vfs.readFile(Entelechy::Path("hello.txt"));
    ASSERT_TRUE(file.valid);
    ASSERT_EQ(file.bytes.size(), testLen);

    for (usize i = 0; i < testLen; ++i)
    {
        ASSERT_EQ(file.bytes[i], static_cast<u8>(testData[i]));
    }

    vfs.clear();
}

TEST(VFS, MemoryMountPoint)
{
    Entelechy::VFS vfs;
    void *memMem =
        Entelechy::DefaultAllocator::alloc(sizeof(Entelechy::MemoryMountPoint), alignof(Entelechy::MemoryMountPoint));
    auto *memMount = std::construct_at(static_cast<Entelechy::MemoryMountPoint *>(memMem));
    const char *memData = "Memory file content";
    memMount->registerFile("mem/test.txt", reinterpret_cast<const u8 *>(memData), 19);
    vfs.mount("/mem", memMount);

    ASSERT_TRUE(vfs.exists(Entelechy::Path("mem/test.txt")));
    auto file = vfs.readFile(Entelechy::Path("mem/test.txt"));
    ASSERT_TRUE(file.valid);

    vfs.clear();
}
