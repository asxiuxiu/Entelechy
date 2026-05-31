# Test 模块（测试框架）

> 路径：`_engine/source/test`

## 一句话职责
轻量级自研测试框架：提供 `TEST()` / `ASSERT_*` 宏、自动注册机制与统一运行入口，零外部依赖。

## 关键文件

| 文件 | 用途 |
|------|------|
| `test_framework.h` | `TestRegistry` 单例、`TEST(Suite,Name)` 宏、`ASSERT_TRUE` / `ASSERT_FALSE` / `ASSERT_EQ` / `ASSERT_NE` 宏 |
| `test_framework.cpp` | `TestRegistry::runAll()` 实现：遍历注册表、按 Suite 分组输出、统计 PASS/FAIL |
| `CMakeLists.txt` | `TestFrameworkLib` 静态库定义，依赖 `CoreLib` |

## 设计原则

- **零 STL 依赖**：仅使用 `<cstdio>` / `<cstdarg>` / `<cstring>`，不引入 `std::function` / `std::vector`
- **编译期自动注册**：`TEST()` 宏生成带静态构造函数的注册对象，程序启动时自动向 `TestRegistry` 登记
- **函数指针调用**：`TestCase::fn` 为 `void(*)()`，避免 `<functional>` 开销与分配器依赖
- **失败即返回**：单个 `TEST` 内部 `ASSERT_*` 失败时打印并 `return`，不影响后续测试执行
- **Suite 分组**：测试按 Suite 名分组输出，提升可读性

## 使用方式

```cpp
#include "test_framework.h"
#include "vfs.h"

TEST(VFS, FileSystemMountPointReadWrite) {
    Entelechy::VFS vfs;
    void* mem = Entelechy::DefaultAllocator::alloc(sizeof(Entelechy::FileSystemMountPoint), alignof(Entelechy::FileSystemMountPoint));
    auto* fsMount = std::construct_at(static_cast<Entelechy::FileSystemMountPoint*>(mem), "build/test_vfs");
    vfs.mount("/test", fsMount);

    const char* data = "Hello from VFS!";
    bool writeOk = fsMount->writeFile("hello.txt", reinterpret_cast<const u8*>(data), 17);
    ASSERT_TRUE(writeOk);

    auto file = vfs.readFile(Entelechy::Path("hello.txt"));
    ASSERT_TRUE(file.valid);
    ASSERT_EQ(file.bytes.size(), 17u);

    vfs.clear();
}
```

## 依赖关系

- 向上依赖：
  - [Base 模块](../base/AGENTS.md)（`DynamicArray`、`usize`、`foundation_types.h`）
- 被依赖：
  - 所有模块的 `tests/` 目录（通过 `target_link_libraries(... TestFrameworkLib)`）
  - [TestRunner 模块](../test_runner/AGENTS.md)（最终可执行文件）
