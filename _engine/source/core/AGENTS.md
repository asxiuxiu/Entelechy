# Core 模块

> 路径：`_engine/source/core`

## 一句话职责
ECS 数据层：World 容器、组件存储、实体查询、类型注册与反射。

## 关键文件
| 文件 | 职责 |
|------|------|
| `world.h` | `World` 类；实体 spawn/destroy、组件 add/remove/get（模板 API） |
| `world.cpp` | 初始化 `initCore()`、内置组件注册、调试用 `printWorld()`、World 析构显式释放组件数组 |
| `type_registry.h/.cpp` | `TypeRegistry` 单例；组件 ID/掩码分配；`REFLECT_COMPONENT` 自动注册宏；组件 JSON 描述 |
| `component_array.h` | `ComponentArray<T>` 组件存储数组（SparseSet + SIMD 对齐 Column） |
| `sparse_set.h/.cpp` | `SparseSet` 分页稀疏数组；独立化的 entity→dense 索引映射 |
| `archetype_chunk.h` | Archetype Chunk 存储接口草图（16KiB SoA，为阶段 4.1 预留） |
| `command_buffer.h` | 命令缓冲；延迟执行实体创建/销毁 |
| `query.h` | `Query<...>` 模板；按组件掩码迭代实体 |
| `types.h` | ECS 基础类型（`Entity` 等），依赖 Foundation 的 `u32`/`usize` |
| `string_id.h` | `StringId` 编译期哈希标识 |
| `small_string.h` | `SmallString` SSO 小字符串优化 |
| `string_format.h` | 类型安全的零分配栈格式化器 |
| `string_intern_pool.h/.cpp` | `StringInternPool` 运行时字符串池与冲突检测 |

## 重要入口
- 改**ECS 核心数据模型**（实体生命周期、组件存储方式） → 动 `world.h`
- 改**组件注册/反射机制** → 动 `type_registry.h/.cpp`
- 改**组件数组存储策略** → 动 `component_array.h`、`sparse_set.h/.cpp`
- 改**实体查询语法** → 动 `query.h`
- 新增**内置组件类型** → 在 `world.cpp` 的 `registerBuiltinTypes()` 中注册
- 预研**Archetype Chunk 存储** → 动 `archetype_chunk.h`

## 依赖关系
- 向上依赖：
  - [Foundation 模块](../foundation/AGENTS.md)（基础类型、平台宏、断言）
  - [Memory 模块](../memory/AGENTS.md)（FrameArena、DynamicArray、[HashMap](../memory/AGENTS.md)、DefaultAllocator）
- 被依赖：
  - [System 模块](../system/AGENTS.md)（Scheduler tick 消费 World 数据）
  - [Bridge 模块](../bridge/AGENTS.md)（AgentBridge 查询/修改 World）
  - [Runtime 模块](../../_game/source/runtime/AGENTS.md)

## 架构决策 / 临时约束
- ECS 处于早期阶段，数据布局未来可能从 AoS 向 SoA 演进
- `World` 使用 `HashMap<ComponentTypeID, IComponentArray*>` 存储组件，已去除 `std::shared_ptr` 与 `std::unordered_map`
- `TypeRegistry` 使用 `HashMap` 替代 `std::unordered_map`，消除了 STL 容器依赖
- `World` 析构时显式 `delete` 所有 `IComponentArray`，生命周期由 World 完全拥有
- `ComponentArray<T>` 内部使用 `SparseSet`（分页稀疏数组）+ `Column<T>`（SIMD 对齐 dense 列）
  - `SparseSet` 每页 1024 slots，按需分配，避免大 EntityID 场景内存爆炸
  - `Column<T>` 分配对齐到 `max(alignof(T), 16)`，为 SIMD 预留
- `REFLECT_COMPONENT` 宏在全局构造期自动注册组件，无需手动维护注册表
- Archetype Chunk 接口已预留（`archetype_chunk.h`），下阶段可无缝接续
