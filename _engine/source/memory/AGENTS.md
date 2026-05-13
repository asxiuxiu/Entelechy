# Memory 模块

> 路径：`_engine/source/memory`

## 一句话职责
内存分配策略：帧分配器、对象池、分配器抽象接口、分配器统计与调试体系。

## 关键文件
| 文件 | 职责 |
|------|------|
| `iallocator.h` | 统一虚接口 `IAllocator`（含可选 `getStats()`），供运行时多态与装饰器使用 |
| `allocator.h` | `DefaultAllocator`（静态方法）；`DefaultAllocatorV`（虚包装）；`GetGlobalAllocator()` / `GetGlobalAllocatorStats()` |
| `debug_allocator_wrapper.h/.cpp` | Debug/Development 构建自动启用的调试装饰器（Canary / Poison / Stats）；Release 完全剥离 |
| `frame_arena.h/.cpp` | `FrameArena` 帧分配器：嵌套回滚、任意对齐、溢出回退、双缓冲预留接口、分配统计 |
| `object_pool.h` | `ObjectPool<T>` 动态对象池：动态分块（1024 slots/block）、分代索引 `PoolHandle`、分配统计 |
| `ALLOCATOR_GUIDE.md` | 分配器选择决策图与快速对照表 |

## 重要入口
- 改**统一分配器接口** → 动 `iallocator.h`
- 改**全局分配器与调试包装** → 动 `allocator.h`、`debug_allocator_wrapper.h/.cpp`
- 改**帧分配器策略**（容量、对齐方式、溢出处理、嵌套作用域、统计） → 动 `frame_arena.h/.cpp`
- 改**对象池实现**（动态扩容、分代句柄、统计） → 动 `object_pool.h`
- 改**分配器统计与诊断面板数据** → 动 `iallocator.h` + 各分配器 `getStats()`
- 改**分配器选择文档** → 动 `ALLOCATOR_GUIDE.md`

## 依赖关系
- 向上依赖：
  - [Base 模块](../base/AGENTS.md)（`usize`、`PLATFORM_WINDOWS`、断言）
- 被依赖：
  - [Core 模块](../core/AGENTS.md)（World 使用 FrameArena / DefaultAllocator）
  - [Math 模块](../math/AGENTS.md)（对齐工具使用 `usize`）
  - 其他模块

## 架构决策 / 临时约束
- `FrameArena` 基于 `DefaultAllocator` + 手动偏移管理，**溢出时自动回退到堆分配**（链表挂接，reset 时统一释放）
- `FrameArena` 支持嵌套 `MemMark` 回滚（O(1)），但 overflow 块内的精确回滚在 Phase A 中暂未完整支持
- `ObjectPool<T>` 使用动态分块（每块 1024 slots），返回 `PoolHandle` 而非裸指针，支持分代失效检测
- `ObjectPool<T>` 同时实现 `IAllocator` 接口（单对象分配，大请求返回 nullptr）
- `DebugAllocatorWrapper` 仅在 `_DEBUG` / `DEBUG` 构建中启用完整功能；Release 构建下类定义完全剥离（零开销）
- **统一统计**：`IAllocator::getStats()` 返回 `AllocatorStats{total, peak, count, active}`；
  `FrameArena` / `ObjectPool` / `DebugAllocatorWrapper` 均已实现；
  `GetGlobalAllocatorStats()` 提供全局汇总快捷接口（为 ImGui 性能面板预留数据）
- 分配器选择决策参考：[ALLOCATOR_GUIDE.md](ALLOCATOR_GUIDE.md)
