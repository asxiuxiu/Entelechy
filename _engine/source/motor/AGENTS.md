# Motor 模块

> 路径：`_engine/source/motor`

## 一句话职责
引擎内置运动系统：基础运动学 System（速度积分）、未来扩展 Motor 驱动框架。

## 关键文件
| 文件 | 职责 |
|------|------|
| `movement_system.h/.cpp` | 引擎内置 System：遍历 `Position` + `Velocity` 组件，执行 `pos += vel * dt` |

## 重要入口
- 改**基础运动行为** → 动 `movement_system.h/.cpp`
- 新增**Motor 驱动类型**（biped / vehicle / aircraft 等）→ 在本模块下新增文件

## 依赖关系
- 向上依赖：
  - [Core 模块](../core/AGENTS.md)（`World`、`Query`、`Scheduler`、`System` 基类）
- 被依赖：
  - [Bridge 模块](../bridge/AGENTS.md)（AgentBridge 注册 MovementSystem）
  - [ImGui 模块](../imgui/AGENTS.md)（Inspector 中观察运动状态）

## 架构决策 / 约束
- `MovementSystem` 是引擎**通用内置系统**，非某个特定游戏的定制逻辑
- 遵循 "System 跟随它关注的领域走" 的分包原则：运动相关 System 集中在 `motor/`
- 未来 Motor 框架（驱动器、输入映射、死亡推算、客户端预测）均在本模块扩展
