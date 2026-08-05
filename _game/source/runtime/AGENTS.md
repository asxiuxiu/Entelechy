# Runtime 模块

> 路径：`_game/source/runtime`

## 一句话职责

游戏运行时入口、主循环驱动、模块初始化调用。

## 关键文件
| 文件 | 职责 |
|------|------|
| `game_runtime.h` | 运行时初始化函数声明 `initRuntime()` |
| `game_runtime.cpp` | 运行时初始化实现；目前为桩 |
| `game_plugin.h/.cpp` | `GamePlugin`：游戏层 Plugin，注册演示系统（Movement/FlyCamera/TransformPropagation/EventCleanup）并生成演示场景（飞行相机 + 立方体阵列 + 贴图地面 + 缩放柱子） |
| `fly_camera_system.h/.cpp` | `FlyCameraSystem` + `FlyCameraTag`：自由飞行相机（WASD + Q/E 升降 + Shift 加速 + 右键拖拽视角），经 `InputQueue` 单例维护 held-key 状态 |
| `render_assets.h` | 演示资产基础设施 `RenderAssets`：VFS（双挂载点兼容项目根/build·bin·Debug 两种 cwd）+ `AssetServer` + `TextureAssetLoader`/`MeshAssetLoader`（`.emesh` 异步加载入口，阶段 3a 注册）+ 三类 `Assets<T>` 存储 + 缓存 Handle；`initRenderAssets()` 幂等构建程序化网格/材质并发起棋盘格贴图异步加载；main 将存储绑定到 Prepare 阶段（`bindAssets`），每帧 `processEvents()` 消费完成事件 |

## 重要入口
- 改**游戏层初始化/关闭逻辑** → 动 `game_runtime.cpp`
- 改**游戏层 Plugin 注册** → 动 `game_plugin.h/.cpp`
- 注意：实际主循环在 `launch/templates/main.cpp`（构建系统生成），不在 RuntimeLib；但主循环已通过 `App` 委托 `app.update(dt)`
- 新增**游戏层特定系统或逻辑** → 新建 `IPlugin` 子类并在 `main.cpp` 中 `app.addPlugin()`

## 依赖关系
- 向上依赖：
  - 所有引擎模块（[Core](../core/AGENTS.md) / [ECS](../ecs/AGENTS.md) / [Window](../window/AGENTS.md) / [Render](../render/AGENTS.md) / [ImGui](../imgui/AGENTS.md) / [Log](../log/AGENTS.md)）
- 被依赖：
  - 无（最顶层，被 main 调用）

## 架构决策
- `_game/` 模拟独立仓库，与 `_engine/` 物理分离，确保引擎核心不依赖游戏层

## 技术债务

> 统一维护于 [TODO.md](../../../../TODO.md)。本模块相关条目包括：Runtime/MainLoop。
