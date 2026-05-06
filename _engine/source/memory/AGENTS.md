# Memory 模块

> 路径：`_engine/source/memory`

## 一句话职责
内存分配器与数据结构：帧分配器、对象池、动态数组、分配器抽象接口。

## 关键文件
| 文件 | 职责 |
|------|------|
| `frame_arena.h` | `FrameArena` 帧分配器声明 |
| `frame_arena.cpp` | `FrameArena` 实现：`alloc()` / `reset()`，基于 malloc 的连续缓冲区 |
| `allocator.h` | 分配器抽象接口（预留） |
| `object_pool.h` | 对象池模板 |
| `dynamic_array.h` | `DynamicArray<T>` 动态数组模板（类似 std::vector 的轻量替代） |

## 重要入口
- 改**帧分配器策略**（容量、对齐方式、溢出处理） → 动 `frame_arena.h/.cpp`
- 改**对象池实现** → 动 `object_pool.h`
- 改**动态数组行为** → 动 `dynamic_array.h`

## 依赖关系
- 向上依赖：
  - 无（最底层）
- 被依赖：
  - [Core 模块](../core/AGENTS.md)（World 使用 DynamicArray 存储实体数据）
  - [System 模块](../system/AGENTS.md)（Scheduler 每帧创建 1MiB FrameArena）
  - 其他模块

## 架构决策 / 临时约束
- `FrameArena` 基于 `std::malloc` + 手动偏移管理，无 fallback（溢出返回 nullptr）
- `DynamicArray` 是自定义轻量 vector 替代，避免引入 STL 到核心数据层
- FrameArena 是 Phase 0 的核心内存策略，后续会引入更复杂的 arena / pool / stack allocator 体系
