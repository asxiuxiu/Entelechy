# System 模块

> 路径：`_engine/source/system`

## 一句话职责
System / Scheduler 骨架、具体游戏系统实现（如 MovementSystem）、系统模块初始化。

## 关键文件
| 文件 | 职责 |
|------|------|
| `scheduler.h` | `System` 抽象基类、`Scheduler` 调度器（注册系统 + 每帧 tick） |
| `movement_system.h/.cpp` | `MovementSystem` 示例系统：Query Position+Velocity 并更新位置 |
| `system_init.cpp` | 系统模块初始化 `initSystem()` |

## 重要入口
- 改**系统调度策略**（执行顺序、并行策略、帧分配器大小） → 动 `scheduler.h`
- 新增/修改**具体游戏系统** → 新增 `*_system.h/.cpp` 并继承 `System`
- 注册新系统到调度器 → 在 [Bridge 模块](../bridge/AGENTS.md) 的 `AgentBridge::init()` 或 [Runtime 模块](../../_game/source/runtime/AGENTS.md) 初始化中调用 `scheduler.registerSystem()`

## 依赖关系
- 向上依赖：
  - [Core 模块](../core/AGENTS.md)（World、Query、组件类型）
  - [Memory 模块](../memory/AGENTS.md)（FrameArena）
- 被依赖：
  - [Bridge 模块](../bridge/AGENTS.md)（AgentBridge 持有 Scheduler）
  - [Runtime 模块](../../_game/source/runtime/AGENTS.md)（主循环驱动 tick）

## 架构决策 / 临时约束
- Scheduler 目前按注册顺序串行执行，每帧创建 1MiB FrameArena 供所有系统共享
- `CommandBuffer` 在 tick 末尾统一 apply，实现延迟创建/销毁实体
- MovementSystem 是 Phase 0 的示例系统，未来会被更多真实系统替代
- Scheduler 目前较简陋，未来会引入多线程 job system 或任务图
