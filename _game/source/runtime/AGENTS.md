# Runtime 模块

> 路径：`_game/source/runtime`

## 一句话职责

游戏运行时入口、主循环驱动、模块初始化调用。

## 关键文件
| 文件 | 职责 |
|------|------|
| `game_runtime.h` | 运行时初始化函数声明 `initRuntime()` |
| `game_runtime.cpp` | 运行时初始化实现；目前为桩 |
| `game_plugin.h/.cpp` | `GamePlugin`：游戏层 Plugin，注册演示系统（Movement/FlyCamera/MaterialTextureBackfill/TransformPropagation/EventCleanup）并生成演示场景（飞行相机 + 方向光（5a）+ 天空设置（5c）+ cooked Sponza 场景；阶段 4c 起场景加载只剩一行 `renderAssets().scene_loader.spawnCookedScene(world, path)` 调用，解析/装配归引擎 Scene 模块） |
| `fly_camera_system.h/.cpp` | `FlyCameraSystem` + `FlyCameraTag`：自由飞行相机（WASD + Q/E 升降 + Shift 加速 + 右键拖拽视角）；held-key 状态经 `IWindow::isKeyDown()` 每帧轮询（2026-08-08 起，修复事件累积在丢事件时永久错位的 WASD 失灵问题），窗口无焦点时清空瞬态输入状态；窗口指针由 main 注入（`GamePlugin::flyCamera().setWindow()`）；初始 yaw/pitch 硬编码与 `GamePlugin::setup()` 的相机出生位姿匹配（当前：Sponza 中庭西端朝 +X） |
| `render_assets.h` | 演示资产基础设施 `RenderAssets`：VFS（三挂载点兼容项目根 / VS 调试器 / 双击 exe 三种 cwd）+ `AssetServer` + `TextureAssetLoader`/`MeshAssetLoader`（`.emesh` 阶段 3a 异步加载入口）+ 三类 `Assets<T>` 存储 + 缓存 Handle + `scene_loader`（阶段 4c：引擎 `SceneLoader` 实例，注入本结构的 VFS/AssetServer/loader/存储，自持 `MaterialAssetLoader` 与场景材质清单；Sponza 专属的 `material_loader`/`scene_materials` 已随迁引擎移除）；`initRenderAssets()` 幂等构建程序化网格/材质并发起棋盘格贴图异步加载（3c 的共享白模 `mat_white` 已随 4b `shade_mode` 退役移除）；main 将存储绑定到 Prepare 阶段（`bindAssets`），每帧 `processEvents()` 消费完成事件 |

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
