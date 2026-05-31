# Core 模块（Foundation 标准库层）

> 路径：`_engine/source/ecs`（最终重命名为 `ecs/`）

## 一句话职责

引擎标准库：固定宽度类型、平台宏、字符串系统、容器模板、内存分配器、数学库。零外部引擎依赖，被所有上层模块共享。

## 关键文件

### 基础类型与容器

| 文件 | 用途 |
|------|------|
| `foundation_types.h` | 标量别名、平台宏、断言层级、导出辅助宏 |
| `string_id.h` | `StringId` 编译期哈希标识 |
| `small_string.h` | `SmallString` SSO 小字符串优化（支持 find/substr/startsWith/endsWith/operator+） |
| `string_format.h` | 类型安全的零分配栈格式化器 |
| `string_intern_pool.h/.cpp` | `StringInternPool` 运行时字符串池与冲突检测 |
| `dynamic_array.h` | `DynamicArray<T, AllocatorT>` 动态数组模板 |
| `hash_map.h` | `HashMap<K,V,Hash,Allocator>` 开放寻址哈希表 |
| `inline_array.h` | `InlineArray<T,N>` 栈上小数组优化 |
| `ring_buffer.h` | `RingBuffer<T,SIZE>` SPSC 无锁环形队列 |
| `fixed_ring_queue.h` | `FixedRingQueue<T,Capacity>` 固定容量环形队列 |
| `path.h` | 路径工具函数 |

### 内存分配

| 文件 | 用途 |
|------|------|
| `allocator.h` | `DefaultAllocator`（静态方法）；`DefaultAllocatorV`（虚包装）；`GetGlobalAllocator()` |
| `iallocator.h` | 统一虚接口 `IAllocator`（含可选 `getStats()`） |
| `memory/frame_arena.h/.cpp` | `FrameArena` 帧分配器：嵌套回滚、任意对齐、溢出回退 |
| `memory/object_pool.h` | `ObjectPool<T>` 动态对象池：动态分块、分代索引 `PoolHandle` |
| `memory/debug_allocator_wrapper.h/.cpp` | Debug/Development 构建自动启用的调试装饰器（Canary / Poison / Stats） |

### 数学库

| 文件 | 用途 |
|------|------|
| `math/math_lib.h/.cpp` | 数学库聚合头文件，复杂数学运算实现 |
| `math/math_config.h` | NaN/Inf 诊断宏开关 |
| `math/vec.h` | 向量（Vec2 / Vec3 / Vec4） |
| `math/mat4.h` | 4×4 列主序矩阵及运算 |
| `math/quat.h` | 四元数及旋转相关运算 |
| `math/aabb.h` | 轴对齐包围盒（AABB） |
| `math/ray.h` | 射线（Ray），含 `intersectAABB` |
| `math/frustum.h` | 视锥体（Frustum） |
| `math/random.h` | 确定性随机数生成器 `RandomXORShift32` |
| `math/align.h` | 内存对齐工具宏/函数 |
| `math/simd.h` | 跨平台 SIMD 抽象层 |
| `math/half.h` | IEEE 754 Float16 |

## 设计原则

- **零依赖**：本模块不 `#include` 引擎任何其他模块的头文件
- **容器通过模板参数接收分配器**：`DynamicArray<T, AllocatorT = DefaultHeapAllocator>`
- **数学类型保持纯 POD**：无 ECS 耦合，无运行时虚函数分发

## 规则

- 禁止向本模块添加上层模块依赖
- 容器保持仅头文件（header-only）或最小 `.cpp` 配套

## 测试

- 模块测试位于 `tests/` 目录
- 测试库名为 `CoreTests`（OBJECT 库），由 [TestRunner](../test_runner/AGENTS.md) 自动收集链接
