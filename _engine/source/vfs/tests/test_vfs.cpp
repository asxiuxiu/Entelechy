#include "test_framework.h"
#include "vfs.h"
#include "mount_point.h"

TEST(VFS, FileSystemMountPointReadWrite) {
    Entelechy::VFS vfs;
    auto* fsMount = new Entelechy::FileSystemMountPoint("build/test_vfs");
    vfs.mount("/test", fsMount);

    const char* testData = "Hello from VFS!";
    usize testLen = 17;
    bool writeOk = fsMount->writeFile("hello.txt", reinterpret_cast<const u8*>(testData), testLen);
    ASSERT_TRUE(writeOk);

    auto file = vfs.readFile(Entelechy::Path("hello.txt"));
    ASSERT_TRUE(file.valid);
    ASSERT_EQ(file.bytes.size(), testLen);

    for (usize i = 0; i < testLen; ++i) {
        ASSERT_EQ(file.bytes[i], static_cast<u8>(testData[i]));
    }

    vfs.clear();
}

TEST(VFS, MemoryMountPoint) {
    Entelechy::VFS vfs;
    auto* memMount = new Entelechy::MemoryMountPoint();
    const char* memData = "Memory file content";
    memMount->registerFile("mem/test.txt", reinterpret_cast<const u8*>(memData), 19);
    vfs.mount("/mem", memMount);

    ASSERT_TRUE(vfs.exists(Entelechy::Path("mem/test.txt")));
    auto file = vfs.readFile(Entelechy::Path("mem/test.txt"));
    ASSERT_TRUE(file.valid);

    vfs.clear();
}
