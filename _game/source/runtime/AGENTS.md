# Runtime 模块

> 路径：`_game/source/runtime`

## 一句话职责
游戏运行时入口、主循环驱动、模块初始化调用。

## 关键文件
| 文件 | 职责 |
|------|------|
| `game_runtime.h` | 运行时初始化函数声明 `initRuntime()` |
| `game_runtime.cpp` | 运行时初始化实现；目前为桩 |
| `game_plugin.h/.cpp` | `GamePlugin`：游戏层 Plugin，注册演示系统（Movement/Rotation/Wobble/ColorChange/TransformPropagation/EventCleanup）并生成演示立方体层级 |

## 重要入口
- 改**游戏层初始化/关闭逻辑** → 动 `game_runtime.cpp`
- 改**游戏层 Plugin 注册** → 动 `game_plugin.h/.cpp`
- 注意：实际主循环在 `launch/templates/main.cpp`（构建系统生成），不在 RuntimeLib；但主循环已通过 `App` 委托 `app.update(dt)`
- 新增**游戏层特定系统或逻辑** → 新建 `IPlugin` 子类并在 `main.cpp` 中 `app.addPlugin()`

## 依赖关系
- 向上依赖：
  - 所有引擎模块（[Core](../core/AGENTS.md) / [System](../system/AGENTS.md) / [Window](../window/AGENTS.md) / [Render](../render/AGENTS.md) / [ImGui](../imgui/AGENTS.md) / [Log](../log/AGENTS.md)）
- 被依赖：
  - 无（最顶层，被 main 调用）

## 架构决策 / 临时约束
- `main.cpp` 是构建时由 `launch/generator.py` 生成的模板，不在本模块目录内
- 未来 Runtime 会接管更多主循环逻辑，把 `main.cpp` 中的引擎代码下沉到这里
- `_game/` 模拟独立仓库，与 `_engine/` 物理分离，确保引擎核心不依赖游戏层
