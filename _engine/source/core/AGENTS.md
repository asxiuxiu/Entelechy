# Core 模块

> 路径：`_engine/source/core`

## 一句话职责
ECS 数据层：World 容器、组件存储、实体查询、类型注册与反射。

## 关键文件
| 文件 | 职责 |
|------|------|
| `world.h` | `World` 类；实体 spawn/destroy、组件 add/remove/get（模板 API） |
| `world.cpp` | 初始化 `initCore()`、内置组件注册、调试用 `printWorld()` |
| `type_registry.h` | `TypeRegistry` 单例；组件 ID/掩码分配；`REFLECT_COMPONENT` 自动注册宏 |
| `type_registry.cpp` | TypeRegistry 实现；组件列表/描述 JSON 序列化 |
| `component_array.h` | `ComponentArray<T>` 组件存储数组（SoA 接口） |
| `command_buffer.h` | 命令缓冲；延迟执行实体创建/销毁 |
| `query.h` | `Query<...>` 模板；按组件掩码迭代实体 |
| `types.h` | ECS 基础类型（`Entity`、`ComponentTypeID` 等） |
| `string_id.h` | `SmallString` 小字符串优化 |
| `string_format.h` | 字符串格式化工具 |

## 重要入口
- 改**ECS 核心数据模型**（实体生命周期、组件存储方式） → 动 `world.h`
- 改**组件注册/反射机制** → 动 `type_registry.h/.cpp`
- 改**组件数组存储策略** → 动 `component_array.h`
- 改**实体查询语法** → 动 `query.h`
- 新增**内置组件类型** → 在 `world.cpp` 的 `registerBuiltinTypes()` 中注册

## 依赖关系
- 向上依赖：
  - [Memory 模块](../memory/AGENTS.md)（FrameArena、DynamicArray）
- 被依赖：
  - [System 模块](../system/AGENTS.md)（Scheduler tick 消费 World 数据）
  - [Bridge 模块](../bridge/AGENTS.md)（AgentBridge 查询/修改 World）
  - [Runtime 模块](../../_game/source/runtime/AGENTS.md)

## 架构决策 / 临时约束
- ECS 处于早期阶段，数据布局未来可能从 AoS 向 SoA 演进
- `World` 使用 `unordered_map<ComponentTypeID, shared_ptr<IComponentArray>>` 存储组件，非极致性能方案
- `REFLECT_COMPONENT` 宏在全局构造期自动注册组件，无需手动维护注册表
