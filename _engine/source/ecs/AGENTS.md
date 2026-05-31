# ECS 模块（ECS 运行时 + 应用框架）

> 路径：`_engine/source/ecs`（最终重命名为 `ecs/`）

## 一句话职责

ECS 运行时框架：World 容器、组件存储、实体查询、类型注册、System 调度器、命令缓冲、App/Plugin 生命周期、Prefab、场景序列化、反射、事件系统、变换传播。

## 关键文件

| 文件 | 职责 |
|------|------|
| `world.h/.cpp` | `World` 类；实体 spawn/destroy/batch、组件 add/remove/get、父子关系管理 |
| `entity_registry.h/.cpp` | `EntityRegistry`：独立的实体 ID 分配器 |
| `type_registry.h/.cpp` | `TypeRegistry` 单例；组件 ID/掩码分配、`REFLECT_COMPONENT` 自动注册宏 |
| `atom_registry.h/.cpp` | `AtomRegistry` 单例；原子类型行为函数注册表 |
| `inspector_reflection.h` | 反射绘制接口：`inspectorDrawComponent`、`inspectorDrawField` |
| `plugin.h` | `IPlugin` 接口：Bevy 风格插件生命周期（build/setup/teardown） |
| `app.h/.cpp` | `App` 顶层容器：聚合 World + Scheduler + Plugin 列表 |
| `prefab.h/.cpp` | `PrefabAsset` / `PrefabInstance` / `PrefabSystem`：实例化-合并策略 |
| `scene_serializer.h/.cpp` | `SceneSerializer`：基于反射的 World JSON 序列化/反序列化 |
| `transform_component.h` | ECS 组件 `Transform`（本地 TRS）/`GlobalTransform`/`TransformTreeChanged` |
| `hierarchy.h` | 层级辅助函数：`hierarchyDepth`、`attachChild`、`detachChild` 等 |
| `transform_propagation_system.h/.cpp` | 引擎内置 System：按层级深度拓扑排序传播本地 Transform 到 GlobalTransform |
| `component_array.h` | `IComponentArray` 接口 + `ComponentArray<T>`（SparseSet + SIMD 对齐 Column） |
| `command_buffer.h` | `CommandBuffer`：延迟执行命令（set/remove/destroy/destroyWithChildren/setParent） |
| `sparse_set.h/.cpp` | `SparseSet` 分页稀疏数组 |
| `archetype_chunk.h` | Archetype Chunk 存储接口草图（16KiB SoA） |
| `query.h` | `Query<...>` 模板；按组件掩码迭代实体 |
| `scheduler.h/.cpp` | `System` 基类、`Scheduler` 调度器；Phase 模型、依赖图拓扑排序、歧义检测、固定时间步 |
| `phase.h` | `DefaultPhase` 枚举（First/PreUpdate/Update/PostUpdate/Last） |
| `system_desc.h` | `SystemDesc`：系统描述符（name/phase/reads/writes/before/after） |
| `events.h` | 内置 ECS 事件组件：`KeyboardEvent`、`ColorChangeEvent`、`DeathEvent` |
| `event_lifetime.h` | `EventLifetime` 组件（frameCreated），标记事件实体存活周期 |
| `event_cleanup_system.h/.cpp` | `EventCleanupSystem`：Last Phase 销毁过期事件实体 |
| `event_buffer.h` | 预留的环形缓冲事件队列 `EventBuffer<T>` |
| `types.h` | ECS 基础类型（`Position`、`Velocity`、`Health`、`NameTag`、`Color`）+ 层级关系组件 |

## 重要入口

- 改**ECS 核心数据模型** → 动 `world.h`、`entity_registry.h`
- 改**组件注册/反射机制** → 动 `type_registry.h/.cpp`
- 改**System 调度策略** → 动 `scheduler.h/.cpp`、`phase.h`、`system_desc.h`
- 改**变换传播行为** → 动 `transform_propagation_system.h/.cpp`
- 改**Prefab 系统** → 动 `prefab.h/.cpp`
- 改**场景序列化** → 动 `scene_serializer.h/.cpp`

## 依赖关系

- 向上依赖：
  - [Core 模块](../base/AGENTS.md)（容器 `DynamicArray` / `HashMap`、字符串工具、数学类型、分配器）
- 被依赖：
  - [Motor 模块](../motor/AGENTS.md)（`MovementSystem` 消费 World 数据）
  - [Bridge 模块](../bridge/AGENTS.md)（AgentBridge 查询/修改 World）
  - [Render 模块](../render/AGENTS.md)（渲染系统消费 ECS 数据）
  - [ImGui 模块](../imgui/AGENTS.md)（Inspector 面板查询 ECS 反射）
  - [Runtime 模块](../../_game/source/runtime/AGENTS.md)

## 测试

- 模块测试位于 `tests/` 目录（`test_world.cpp` 等）
- 测试库名为 `EcsTests`（OBJECT 库），由 [TestRunner](../test_runner/AGENTS.md) 自动收集链接
