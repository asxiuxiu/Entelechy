# VFS 模块

> 路径：`_engine/source/vfs`

## 一句话职责

虚拟文件系统：提供统一的文件读写接口，支持多后端挂载（文件系统、内存），按挂载优先级叠加路径空间。

## 关键文件

| 文件 | 用途 |
|------|------|
| `vfs.h/.cpp` | `VFS` 类：挂载点管理（mount/unmount/clear）、文件存在查询、读写分发；后挂载者优先 |
| `mount_point.h/.cpp` | `IMountPoint` 抽象接口；`FileSystemMountPoint` 本地磁盘挂载；`MemoryMountPoint` 内存文件挂载 |
| `vfs_types.h` | `FileData` 结构体（字节数组 + valid 标志） |
| `CMakeLists.txt` | `VFSLib` 静态库定义，依赖 `CoreLib` |

## 重要入口

- 改**挂载策略或优先级规则** → 动 `vfs.cpp`
- 改**文件读写接口** → 动 `mount_point.h` / `IMountPoint`
- 改**本地文件系统实现**（路径拼接、目录创建、跨平台兼容） → 动 `mount_point.cpp`（`FileSystemMountPoint`）
- 改**内存文件实现** → 动 `mount_point.cpp`（`MemoryMountPoint`）
- 新增**挂载后端类型**（如 ZIP、网络、包文件） → 继承 `IMountPoint` 并在本模块新增文件

## 依赖关系

- 向上依赖：
  - [Core 模块](../core/AGENTS.md)（`DynamicArray`、`HashMap`、`SmallString`、`StringId`、`Path`）
- 被依赖：
  - 未来可能被 Asset、Resource 等上层模块依赖

## 架构决策

- 后挂载者优先：VFS 内部按挂载顺序从后向前遍历，后 mount 的挂载点覆盖同名路径
- `MemoryMountPoint` 内部使用 `HashMap<SmallString, DynamicArray<u8>>` 存储文件内容，零外部依赖
- `IMountPoint` 默认 `canWrite() = false`，只读后端无需重写写操作
- `VFS` 析构和 `clear()` 会自动 `delete` 所有挂载点后端，调用方无需手动管理生命周期

## 技术债务

> 统一维护于 [TODO.md](../../../../TODO.md)。本模块相关条目包括：VFS/FileSystemIO。

## 测试

- 模块测试位于 `tests/` 目录（`test_vfs.cpp`）
- 测试库名为 `VFSTests`（OBJECT 库），由 [TestRunner](../test_runner/AGENTS.md) 自动收集链接
- 测试覆盖：`FileSystemMountPoint` 读写、`MemoryMountPoint` 注册与读取
