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
| `string.h` | `BasicString<AllocatorT>` SSO 小字符串优化模板；默认别名 `String`。支持 `reserve`/`capacity`、`StringView` 交互、分配器参数化 |
| `string_view.h` | `StringView` 零拷贝只读字符串引用（`data`/`length`/`substr`/`find`/`startsWith`/`endsWith`） |
| `string_format.h` | 类型安全的零分配栈格式化器（支持 `StringView` 参数） |
| `string_intern_pool.h/.cpp` | `StringInternPool` 运行时字符串池与冲突检测 |
| `dynamic_array.h` | `DynamicArray<T, AllocatorT>` 动态数组模板 |
| `hash_map.h` | `HashMap<K,V,Hash,Allocator>` 开放寻址哈希表 |
| `algorithm/radix_sort.h` | 稳定 64-bit LSD 基数排序 `radixSort64<T, KeyFunc>`，用于 `SortKey` 等固定位宽整数键 |
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

## 基础库优先

本模块提供的基础库设施必须优先使用，禁止在已覆盖领域重复引入 STL 或自行实现：

| 基础库设施 | 替代目标 | 说明 |
|---|---|---|
| `DynamicArray<T>` | `std::vector<T>` | 带可插拔分配器的动态数组 |
| `HashMap<K,V>` / `HashSet<K>` | `std::unordered_map` / `std::unordered_set` | 开放寻址哈希表 |
| `SmallString` | `std::string` | SSO 小字符串 |
| `formatString` | `std::format` / `snprintf` | 零堆分配格式化 |
| `Vec2/Vec3/Vec4/Mat4/Quat` 等 | `glm::` | 数学库 |
| `DefaultAllocator` / `IAllocator` | 裸 `new/delete` / `malloc/free` | 对齐分配器 |
| `ObjectPool<T>` | 裸 `new/delete`（对象池场景） | 带代际安全检查的对象池 |
| `FrameArena` | 临时/帧级堆分配 | O(1) 重置的帧分配器 |

**底线**：基础库已有对应实现时，不允许以"习惯""省事"为由绕回 STL 或裸分配。若接口确实不满足需求，先讨论扩展基础库，而非在业务代码里开例外。

## 字符串体系

引擎字符串分三种**不可互相替代**的角色。选错角色会导致性能劣化或架构污染。

**决策口诀**：先问"要不要存进组件/字典键？" → 要，用 `StringId`；再问"要不要修改/拼接？" → 要，用 `String`；否则默认 `StringView`。

| 角色 | 类型 | 什么时候用 | 绝对不能做的事 |
|------|------|-----------|--------------|
| 标识与键 | `StringId` | 组件名、字典键、uniform 名、系统名、工具名、插件名 | ❌ 不拼接、不 `find`/`substr`、不直接 `StringId(const char*)` |
| 只读引用 | `StringView` | 函数参数只读不存、子串、格式化参数 | ❌ 不存储、不当 `HashMap` 键、不传给要 `\0` 结尾的 C API |
| 局部可变文本 | `String` | 动态拼接、文件路径构建、UI 文本 | ❌ 不放入 ECS 组件、不当热路径 `HashMap` 键 |

**`StringId` 构造（硬性约束）**：
- 编译期常量 → `"Transform"_sid`（UDL，零开销）
- 运行时字符串 → `StringInternPool::instance().intern(str)`（唯一合法来源）
- `StringId` 没有 `.empty()` / `.c_str()` → 判断空用 `id.value() == 0`，反查用 `resolve(id)`

**已 StringId 化的模块**（新增模块必须遵循）：TypeRegistry、AtomRegistry、ToolRegistry、Material+RHI、Scheduler、Plugin、VFS MemoryMountPoint。

**日志是例外**：日志消息不走 `String`，用栈缓冲 `char buf[1024]` + `LogEntry`（`const char*` 借用）+ `FixedRingQueue` 环形历史。日志分类用 `StringId`（`LogCategory`）。

## 测试

- 模块测试位于 `tests/` 目录
- 测试库名为 `CoreTests`（OBJECT 库），由 [TestRunner](../test_runner/AGENTS.md) 自动收集链接
