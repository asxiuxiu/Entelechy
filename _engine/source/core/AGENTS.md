# Core 模块

> 路径：`_engine/source/core`

## 一句话职责
ECS 运行时框架：World 容器、组件存储、实体查询、类型注册、System 调度器、命令缓冲。

## 关键文件
| 文件 | 职责 |
|------|------|
| `world.h/.cpp` | `World` 类；实体 spawn/destroy/batch、组件 add/remove/get（模板 + type-erased API）、父子关系管理 |
| `entity_registry.h/.cpp` | `EntityRegistry`：独立的实体 ID 分配器（generation + alive slot）、批量预分配 `allocateIDs` |
| `type_registry.h/.cpp` | `TypeRegistry` 单例；组件 ID/掩码分配（跨翻译单元安全的 `TypeIDStorage`）、`REFLECT_COMPONENT` 自动注册宏、组件 JSON 描述 |
| `atom_registry.h` | `AtomRegistry` 单例；原子类型（`f32`、`bool`、`StringId` 等）的行为函数注册表，服务 Inspector 与序列化 |
| `inspector_reflection.h` | 反射绘制接口：`inspectorDrawComponent`、`inspectorDrawField`（由 ImGui 模块具体实现） |
| `plugin.h` | `IPlugin` 接口：Bevy 风格插件生命周期（build/setup/teardown） |
| `app.h/.cpp` | `App` 顶层容器：聚合 World + Scheduler + Plugin 列表，管理生命周期 |
| `prefab.h/.cpp` | `PrefabAsset` / `PrefabInstance` / `PrefabSystem`：分支 C 实例化-合并策略，支持嵌套层级与覆盖 |
| `scene_serializer.h/.cpp` | `SceneSerializer`：基于反射的 World JSON 序列化/反序列化，无需手写 per-component 代码 |
| `component_array.h` | `IComponentArray` 接口 + `ComponentArray<T>`（SparseSet + SIMD 对齐 Column）、组件生命周期钩子预留 |
| `command_buffer.h` | `CommandBuffer`：延迟执行命令（set/remove/destroy/destroyWithChildren/setParent）、batch 语义、自定义分配 |
| `sparse_set.h/.cpp` | `SparseSet` 分页稀疏数组；独立化的 entity→dense 索引映射 |
| `archetype_chunk.h` | Archetype Chunk 存储接口草图（16KiB SoA，为阶段 4.1 预留） |
| `query.h` | `Query<...>` 模板；按组件掩码迭代实体 |
| `scheduler.h/.cpp` | `System` 基类、`Scheduler` 调度器；Phase 模型（First/PreUpdate/Update/PostUpdate/Last）、依赖图拓扑排序、歧义检测、固定时间步、管理 FrameArena 与 CommandBuffer |
| `phase.h` | `DefaultPhase` 枚举（First/PreUpdate/Update/PostUpdate/Last） |
| `system_desc.h` | `SystemDesc`：系统描述符（name/phase/reads/writes/before/after） |
| `events.h` | 内置 ECS 事件组件：`KeyboardEvent`、`ColorChangeEvent`、`DeathEvent` |
| `event_lifetime.h` | `EventLifetime` 组件（frameCreated），标记事件实体存活周期 |
| `event_cleanup_system.h/.cpp` | `EventCleanupSystem`：Last Phase 销毁过期事件实体 |
| `event_buffer.h` | 预留的环形缓冲事件队列 `EventBuffer<T>`（分支 B 备选） |
| `types.h` | ECS 基础类型（`Position`、`Velocity`、`Health`、`NameTag`、`Color`）+ 层级关系组件（`ChildOf`、`Children`） |

## 重要入口
- 改**ECS 核心数据模型**（实体生命周期、组件存储方式） → 动 `world.h`、`entity_registry.h`
- 改**组件注册/反射机制** → 动 `type_registry.h/.cpp`
- 改**组件数组存储策略** → 动 `component_array.h`、`sparse_set.h/.cpp`
- 改**实体查询语法** → 动 `query.h`
- 改**System 调度策略** → 动 `scheduler.h/.cpp`、`phase.h`、`system_desc.h`
- 改**事件总线** → 动 `events.h`、`event_lifetime.h`、`event_cleanup_system.h/.cpp`
- 新增**Phase/依赖图** → 在 `scheduler.cpp` 中通过 Kahn 算法拓扑排序 + 双重循环歧义检测
- 改**命令缓冲行为** → 动 `command_buffer.h`
- 新增**内置组件类型** → 在 `types.h` 中定义并使用 `REFLECT_COMPONENT` 宏
- 新增**Plugin** → 实现 `IPlugin` 子类并在 `App` 中 `addPlugin()`
- 新增**Prefab 系统** → 使用 `PrefabSystem::instantiate()` + `PrefabInstance` 组件
- 新增**场景序列化** → 使用 `SceneSerializer::saveToFile()` / `loadFromFile()`
- 预研**Archetype Chunk 存储** → 动 `archetype_chunk.h`

## 依赖关系
- 向上依赖：
  - [Base 模块](../base/AGENTS.md)（容器 `DynamicArray` / `HashMap`、字符串工具）
  - [Memory 模块](../memory/AGENTS.md)（`FrameArena`、`DefaultAllocator`）
- 被依赖：
  - [Motor 模块](../motor/AGENTS.md)（`MovementSystem` 消费 World 数据）
  - [Math 模块](../math/AGENTS.md)（`TransformPropagationSystem` 消费 World 数据）
  - [Bridge 模块](../bridge/AGENTS.md)（AgentBridge 查询/修改 World）
  - [Runtime 模块](../../_game/source/runtime/AGENTS.md)

## 架构决策 / 临时约束
- ECS 处于早期阶段，数据布局未来可能从 AoS 向 SoA 演进
- `World` 使用 `HashMap<ComponentTypeID, IComponentArray*>` 存储组件，已去除 `std::shared_ptr` 与 `std::unordered_map`
- `TypeRegistry` 使用 `HashMap` 替代 `std::unordered_map`，`ComponentDesc` 使用 `DynamicArray<FieldDesc>` 替代 `std::vector`，已完全消除 STL 容器依赖
- `TypeIDStorage<T>::s_id` 使用 C++17 `inline static` 解决跨翻译单元的 `static` 局部变量 ODR 问题
- `World` 析构时显式 `delete` 所有 `IComponentArray`，生命周期由 World 完全拥有
- `EntityRegistry` 从 `World` 中独立拆分，专职实体 ID 分配与回收；`World` 可选择内部拥有或外部注入
- `CommandBuffer` 使用自定义分配（`DefaultAllocator`）替代 `std::unique_ptr`，支持 batch 语义与 `destroyWithChildren` 递归销毁
- `ComponentArray<T>` 内部使用 `SparseSet`（分页稀疏数组）+ `Column<T>`（SIMD 对齐 dense 列）
  - `SparseSet` 每页 1024 slots，按需分配，避免大 EntityID 场景内存爆炸
  - `Column<T>` 分配对齐到 `max(alignof(T), 16)`，为 SIMD 预留
- `REFLECT_COMPONENT` 宏在全局构造期自动注册组件，无需手动维护注册表
- `AtomRegistry` 为 Inspector 提供零硬编码字段绘制能力：原子类型走函数指针表，复合类型（`Vec3`/`Quat`/`Mat4`）走 `TypeRegistry` 递归展开
- `App` 作为顶层运行时容器：持有 `World` + `Scheduler` + `DynamicArray<IPlugin*>`，按 `build()` → `setup()` → `update()` → `teardown()` 驱动生命周期
- `PrefabSystem` 采用分支 C（instantiate-and-merge）：`PrefabAsset` 存储在系统外部，实例化后实体为普通 ECS 实体，零运行时开销；`PrefabInstance` 组件仅作溯源标记
- `SceneSerializer` 利用 `TypeRegistry` + `AtomRegistry` 递归序列化组件字段，新增组件无需修改序列化代码
- `Scheduler` 支持 Phase 模型 + 固定时间步：`tick(raw_dt)` 内部按 `1/60s` 累积，执行 `N` 次固定步长 `tickFixed`；同 Phase 内基于 `before`/`after` 声明进行拓扑排序（Kahn 算法）
- 歧义检测：`build()` 时对同一 Phase 内数据冲突（writes 交集 或 write-read 交集）但未声明 `before`/`after` 的系统对输出 `[Scheduler Ambiguity]` 警告
- 事件即 ECS 实体：`KeyboardEvent` 等作为普通组件附加到事件实体；`EventCleanupSystem` 在 `Last` Phase 统一销毁 `frameCreated <= currentFrame` 的事件，保证事件存活恰好一帧
- `World` 持有 `currentFrame`（由 `Scheduler::tickFixed` 在每次逻辑帧前设置），供 `EventCleanupSystem` 判断事件生命周期
- Archetype Chunk 接口已预留（`archetype_chunk.h`），下阶段可无缝接续

## 测试

- 模块测试位于 `tests/` 目录（`test_world.cpp` 等）
- 测试库名为 `CoreTests`（OBJECT 库），由 [TestRunner](../test_runner/AGENTS.md) 自动收集链接
- 测试覆盖：World spawn/destroy、CommandBuffer destroyWithChildren、批量 spawn + 层级
