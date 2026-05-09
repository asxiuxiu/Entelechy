# Memory 模块

> 路径：`_engine/source/memory`

## 一句话职责
内存分配器与数据结构：帧分配器、对象池、动态数组、哈希表、内联数组、无锁环形队列、分配器抽象接口、分配器统计与调试体系。

## 关键文件
| 文件 | 职责 |
|------|------|
| `iallocator.h` | 统一虚接口 `IAllocator`（含可选 `getStats()`），供运行时多态与装饰器使用 |
| `allocator.h` | `DefaultAllocator`（静态方法）；`DefaultAllocatorV`（虚包装）；`GetGlobalAllocator()` / `GetGlobalAllocatorStats()` |
| `debug_allocator_wrapper.h/.cpp` | Debug/Development 构建自动启用的调试装饰器（Canary / Poison / Stats）；Release 完全剥离 |
| `frame_arena.h/.cpp` | `FrameArena` 帧分配器：嵌套回滚、任意对齐、溢出回退、双缓冲预留接口、分配统计 |
| `object_pool.h` | `ObjectPool<T>` 动态对象池：动态分块（1024 slots/block）、分代索引 `PoolHandle`、分配统计 |
| `dynamic_array.h` | `DynamicArray<T, AllocatorT>` 动态数组模板（类似 std::vector 的轻量替代） |
| `hash_map.h` | `HashMap<K,V,Hash,Allocator>` 开放寻址哈希表；`HashSet<K>` 薄封装 |
| `inline_array.h` | `InlineArray<T,N>` 栈上小数组优化（前 N 个元素内联） |
| `ring_buffer.h` | `RingBuffer<T,SIZE>` SPSC 无锁环形队列（单生产者-单消费者） |
| `ALLOCATOR_GUIDE.md` | 分配器选择决策图与快速对照表 |

## 重要入口
- 改**统一分配器接口** → 动 `iallocator.h`
- 改**全局分配器与调试包装** → 动 `allocator.h`、`debug_allocator_wrapper.h/.cpp`
- 改**帧分配器策略**（容量、对齐方式、溢出处理、嵌套作用域、统计） → 动 `frame_arena.h/.cpp`
- 改**对象池实现**（动态扩容、分代句柄、统计） → 动 `object_pool.h`
- 改**动态数组行为** → 动 `dynamic_array.h`
- 改**哈希表实现**（开放寻址、线性探测） → 动 `hash_map.h`
- 改**内联数组/无锁队列** → 动 `inline_array.h` / `ring_buffer.h`
- 改**分配器统计与诊断面板数据** → 动 `iallocator.h` + 各分配器 `getStats()`
- 改**分配器选择文档** → 动 `ALLOCATOR_GUIDE.md`

## 依赖关系
- 向上依赖：
  - [Foundation 模块](../foundation/AGENTS.md)（`usize`、`PLATFORM_WINDOWS`、断言）
- 被依赖：
  - [Core 模块](../core/AGENTS.md)（World 使用 DynamicArray / HashMap / SparseSet 存储实体与组件数据）
  - [Math 模块](../math/AGENTS.md)（对齐工具使用 `usize`）
  - [System 模块](../system/AGENTS.md)（Scheduler 每帧创建 1MiB FrameArena）
  - 其他模块

## 架构决策 / 临时约束
- `FrameArena` 基于 `DefaultAllocator` + 手动偏移管理，**溢出时自动回退到堆分配**（链表挂接，reset 时统一释放）
- `FrameArena` 支持嵌套 `MemMark` 回滚（O(1)），但 overflow 块内的精确回滚在 Phase A 中暂未完整支持
- `ObjectPool<T>` 使用动态分块（每块 1024 slots），返回 `PoolHandle` 而非裸指针，支持分代失效检测
- `ObjectPool<T>` 同时实现 `IAllocator` 接口（单对象分配，大请求返回 nullptr）
- `DebugAllocatorWrapper` 仅在 `_DEBUG` / `DEBUG` 构建中启用完整功能；Release 构建下类定义完全剥离（零开销）
- `HashMap` 采用开放寻址 + 线性探测，容量保持 2 的幂，负载因子 50% 时自动扩容；**暂不支持删除操作**
- `InlineArray<T,N>` 前 N 个元素存储在结构体内联缓冲区，超出后才向分配器申请堆内存
- `RingBuffer<T,SIZE>` 为 SPSC 无锁队列，有效容量 `SIZE - 1`（留一个 slot 区分空/满）
- `DynamicArray` 是自定义轻量 vector 替代，避免引入 STL 到核心数据层
- **统一统计**：`IAllocator::getStats()` 返回 `AllocatorStats{total, peak, count, active}`；
  `FrameArena` / `ObjectPool` / `DebugAllocatorWrapper` 均已实现；
  `GetGlobalAllocatorStats()` 提供全局汇总快捷接口（为 ImGui 性能面板预留数据）
- 分配器选择决策参考：[ALLOCATOR_GUIDE.md](ALLOCATOR_GUIDE.md)
