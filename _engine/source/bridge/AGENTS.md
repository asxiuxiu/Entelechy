# Bridge 模块

> 路径：`_engine/source/bridge`

## 一句话职责

AgentBridge：外部 Agent/工具与引擎 ECS 世界的交互桥梁，提供结构化 JSON-RPC 风格的工具调用接口。

## 关键文件
| 文件 | 职责 |
|------|------|
| `agent_bridge.h` | `AgentBridge` 类声明；聚合 World + Scheduler + MovementSystem |
| `agent_bridge.cpp` | AgentBridge 实现；注册 queryEntities / getComponent / setComponent / stepWorld / getWorldSummary 等工具 |
| `tool_registry.h` | `ToolRegistry` 单例；`ToolDesc` 结构体；`REFLECT_TOOL` 自动注册宏 |
| `tool_registry.cpp` | ToolRegistry 实现；内置 listTools / describeTool / listComponents / describeComponent 工具 |

## 重要入口
- 改**Agent 可调用工具集**（新增/修改外部接口） → 动 `agent_bridge.cpp` 的 `init()` 方法
- 改**工具注册机制或元数据格式** → 动 `tool_registry.h/.cpp`
- 改**组件反射查询结果格式** → 动 `agent_bridge.cpp` 的 JSON 序列化逻辑

## 依赖关系
- 向上依赖：
  - [ECS 模块](../ecs/AGENTS.md)（World、TypeRegistry、组件类型）
  - [Motor 模块](../motor/AGENTS.md)（MovementSystem）
- 被依赖：
  - [Runtime 模块](../../_game/source/runtime/AGENTS.md)（初始化时创建 AgentBridge）

## 架构决策
- JSON 解析与工具回调全部使用 `SmallString`，已消除 `std::string` 依赖
- `REFLECT_TOOL` 宏支持自动注册工具，避免手动维护注册列表

## 技术债务

> 统一维护于 [TODO.md](../../../../TODO.md)。本模块相关条目包括：Bridge/AgentBridgeCoupling、Bridge/JSONParser。
