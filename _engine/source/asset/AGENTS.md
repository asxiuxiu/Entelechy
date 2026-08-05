# Asset 模块

> 路径：`_engine/source/asset`

## 一句话职责

资源管理基础设施：类型安全句柄、密集存储、引用计数、单线程异步加载调度。

## 关键文件
| 文件 | 职责 |
|------|------|
| `asset_types.h` | `AssetLoadState` 枚举、资源类型 ID 定义 |
| `mesh_asset.h` | `MeshAsset`（交错顶点流 position/normal/uv/tangent + 索引 + AABB，`computeBounds()`） |
| `material_asset.h` | `MaterialAsset` 占位类型（`Handle<MaterialAsset>` 模板参数；字段待后续阶段填充） |
| `texture_asset.h` | `TextureAsset`（RGBA8 像素 + 尺寸，stb_image 解码约定：左上原点） |
| `mesh_primitives.h` | 程序化网格构建器：`buildCubeMesh()`（24 顶点六面立方体）/ `buildGroundMesh()`（XZ 平面 + UV 平铺），Prepare fallback 与游戏 demo 共用 |
| `asset_handle.h` | `Handle<T>` 模板句柄（index + generation），8 字节 POD |
| `handle_table.h` | `HandleTable<T>` 密集存储 + ABA 防护 + 引用计数表 + free list |
| `assets.h` | `Assets<T>` 类型安全门面，提供 insert/get/remove/allocateEmpty/fill |
| `asset_loader.h` | `IAssetLoader<T>` 加载器接口，每种资源类型实现一个 |
| `texture_asset_loader.h/.cpp` | `TextureAssetLoader`：stb_image 解码 PNG/JPEG 等 → `TextureAsset`（RGBA8） |
| `asset_server.h/.cpp` | `AssetServer`：单后台线程加载调度、互斥锁事件队列、同步/异步加载 |

## 重要入口
- 新增资源类型 → 定义数据类型 + 实现 `IAssetLoader<T>`
- 发起加载 → `AssetServer::loadSync()` 或 `loadAsync()`
- 消费异步结果 → 主线程每帧调用 `AssetServer::processEvents()`
- 释放资源 → `AssetServer::unload(handle, storage)`（简化路径：手动卸载）
- 热重载 → `AssetServer::reload()`（复用已有 Handle，替换数据）

## 依赖关系
- 向上依赖：
  - [Core 模块](../core/AGENTS.md)（foundation_types、DynamicArray、HashMap、Path、SmallString）
  - [VFS 模块](../vfs/AGENTS.md)（虚拟文件系统读取）
  - [Log 模块](../log/AGENTS.md)（loader 错误日志，PRIVATE）
  - stb（Conan `stb/cci.20230920`，stb_image 解码，PRIVATE）
- 被依赖：
  - [Render 模块](../render/AGENTS.md)（纹理、网格、着色器资源）

## 架构决策
- `Assets<T>` 是独立存储，通过 `Handle<T>` 被组件引用。后续 ECS 演进后可把 `Assets<T>` 注册为 World 级全局数据。
- `T` 必须可默认构造：`HandleTableSlot<T>` 使用 `DynamicArray` 管理，resize 时默认构造元素。所有引擎资源类型应满足此约束。

## 技术债务

> 统一维护于 [TODO.md](../../../../TODO.md)。本模块相关条目包括：Asset/AssetServerThreading、Asset/HandleTableDefaultConstruct、Asset/PathDeduplication、Asset/HotReload、Asset/ReferenceCounting。

## 测试
- 模块测试位于 `tests/test_asset.cpp`（Handle/HandleTable/Assets/AssetServer）与 `tests/test_asset_types.cpp`（MeshAsset AABB/顶点步长、TextureAssetLoader PNG 解码与失败路径）
- 测试库名为 `AssetTests`（OBJECT 库），由 [TestRunner](../test_runner/AGENTS.md) 自动收集链接
- 覆盖：Handle 有效性、HandleTable ABA 防护、free list、引用计数、Assets insert/remove、AssetServer sync/async/reload
