# Asset 模块

> 路径：`_engine/source/asset`

## 一句话职责
资源管理基础设施：类型安全句柄、密集存储、引用计数、单线程异步加载调度。

## 关键文件
| 文件 | 职责 |
|------|------|
| `asset_types.h` | `AssetLoadState` 枚举、资源类型 ID 定义 |
| `asset_handle.h` | `Handle<T>` 模板句柄（index + generation），8 字节 POD |
| `handle_table.h` | `HandleTable<T>` 密集存储 + ABA 防护 + 引用计数表 + free list |
| `assets.h` | `Assets<T>` 类型安全门面，提供 insert/get/remove/allocateEmpty/fill |
| `asset_loader.h` | `IAssetLoader<T>` 加载器接口，每种资源类型实现一个 |
| `asset_server.h/.cpp` | `AssetServer`：单后台线程加载调度、互斥锁事件队列、同步/异步加载 |

## 重要入口
- 新增资源类型 → 定义数据类型 + 实现 `IAssetLoader<T>`
- 发起加载 → `AssetServer::loadSync()` 或 `loadAsync()`
- 消费异步结果 → 主线程每帧调用 `AssetServer::processEvents()`
- 释放资源 → `AssetServer::unload(handle, storage)`（简化路径：手动卸载）
- 热重载 → `AssetServer::reload()`（复用已有 Handle，替换数据）

## 依赖关系
- 向上依赖：
  - [Base 模块](../base/AGENTS.md)（foundation_types、DynamicArray、HashMap、Path、SmallString）
  - [VFS 模块](../vfs/AGENTS.md)（虚拟文件系统读取）
- 被依赖：
  - [Render 模块](../render/AGENTS.md)（纹理、网格、着色器资源）

## 架构决策 / 临时约束
- **简化路径（Phase 5）**：单后台线程 + 互斥锁队列。未来升级为 ThreadPool + lock-free MPSC channel。
- **无 ECS Resource 集成**：当前 ECS 没有 Resource 概念。`Assets<T>` 是独立存储，通过 `Handle<T>` 被组件引用。后续 ECS 演进后可把 `Assets<T>` 注册为 World 级全局数据。
- **引用计数是顾问式的**：`incrementRef/decrementRef` 提供计数，但 `unload()` 不自动检查计数。自动回收在 Phase 8+ 通过事件驱动批量回收实现。
- **T 必须可默认构造**：`HandleTableSlot<T>` 使用 `DynamicArray` 管理，resize 时默认构造元素。所有引擎资源类型应满足此约束。
- **无路径去重**：同一路径多次 `loadAsync()` 会创建多个 Handle。去重缓存和 Cook 管线在 Phase 8+ 实现。
- **无文件系统 Watch**：热重载目前只能手动 `reload()` 触发。自动 Watch + 脏标记在 Phase 8+ 实现。

## 测试
- 模块测试位于 `tests/test_asset.cpp`
- 测试库名为 `AssetTests`（OBJECT 库），由 [TestRunner](../test_runner/AGENTS.md) 自动收集链接
- 覆盖：Handle 有效性、HandleTable ABA 防护、free list、引用计数、Assets insert/remove、AssetServer sync/async/reload
