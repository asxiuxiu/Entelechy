# Base 模块

> 路径：`_engine/source/base`

## 一句话职责
引擎标准库：固定宽度类型、平台宏、字符串系统、容器模板。零依赖，被所有上层模块共享。

## 关键文件

| 文件 | 用途 |
|------|------|
| `foundation_types.h` | 标量别名、平台宏、断言层级、导出辅助宏 |
| `string_id.h` | `StringId` 编译期哈希标识 |
| `small_string.h` | `SmallString` SSO 小字符串优化（支持 find/substr/startsWith/endsWith/operator+） |
| `string_format.h` | 类型安全的零分配栈格式化器 |
| `string_intern_pool.h/.cpp` | `StringInternPool` 运行时字符串池与冲突检测 |
| `dynamic_array.h` | `DynamicArray<T, AllocatorT>` 动态数组模板（支持 swap/insert/removeAt/find/initializer_list） |
| `hash_map.h` | `HashMap<K,V,Hash,Allocator>` 开放寻址哈希表 |
| `inline_array.h` | `InlineArray<T,N>` 栈上小数组优化 |
| `ring_buffer.h` | `RingBuffer<T,SIZE>` SPSC 无锁环形队列 |
| `fixed_ring_queue.h` | `FixedRingQueue<T,Capacity>` 固定容量环形队列（自动覆盖最老元素，支持遍历） |

## 设计原则

- **零依赖**：本模块不 `#include` 引擎任何其他模块的头文件
- **零运行时状态**（`foundation_types.h` 部分）：仅包含纯编译期宏和类型别名
- **容器通过模板参数接收分配器**：`DynamicArray<T, AllocatorT = DefaultHeapAllocator>`，不直接依赖 `memory/`

## 规则

- 禁止向本模块添加上层模块依赖
- 容器保持仅头文件（header-only）或最小 `.cpp` 配套
