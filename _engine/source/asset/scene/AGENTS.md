# Scene 模块

> 路径：`_engine/source/asset/scene`

## 一句话职责

cooked 场景加载入口：解析引擎工具 mesh_cooker 的自有格式（`scene.json` 场景清单 + `.emat` 材质），异步加载 mesh/材质/贴图并 spawn ECS 实体（渲染管线阶段 4c 自游戏侧迁入，D5：格式归引擎、解析也归引擎）。

## 关键文件
| 文件 | 职责 |
|------|------|
| `public/scene_loader.h` | `SceneLoader`（构造注入 VFS / `AssetServer` / mesh+texture loader / 三类 `Assets<T>`，自持 `MaterialAssetLoader` 与场景唯一材质清单）、`SceneSpawnResult`、`MaterialTextureBackfillSystem`（ECS System 薄封装，不碰组件） |
| `private/scene_loader.cpp` | `spawnCookedScene()`：core `JsonCursor` 固定 schema 解析（无 JSON 库）→ 每实体 `loadAsync` `.emesh`、按 `.emat` 路径去重（HashMap）`loadAsync` 材质并 spawn（烘焙 `GlobalTransform` + `MeshAssetRef` + 各自 `MaterialAssetRef` + `WorldAABB`，局部盒经 `AABB::transformed` 转世界盒）；`backfillMaterialTextures()` 每帧扫场景材质，对「贴图 path 非空且 Handle 无效」的已到达材质发 `loadAsync` 回填 baseColor/normal/MR Handle |

## 重要入口
- 加载 cooked 场景 → `SceneLoader::spawnCookedScene(world, scenePath)`，游戏侧只传场景路径
- 贴图 Handle 回填 → 游戏侧把 `MaterialTextureBackfillSystem`（持有 `SceneLoader&`）注册到 Update 阶段，每帧驱动 `backfillMaterialTextures()`

## 架构决策
- **参数注入，不持有游戏侧全局**：VFS / `AssetServer` / loader / `Assets<T>` 全部由调用方（当前是游戏侧 `RenderAssets`）注入；仅 `MaterialAssetLoader` 由 `SceneLoader` 自持（`.emat` 只经此 loader 消费）。
- **normal/MR 只加载不采样（D4）**：贴图数据落位到 `Assets<TextureAsset>`、Handle 回填进 `MaterialAsset`，但 Prepare 不绑定、shader 无消费端（光照属阶段 5）。
- **回填为每帧轮询的过渡机制**（`Assets<T>` 不支持遍历，只能另存 Handle 清单），改事件驱动的债务已记 TODO.md。
- **Prepare 对称守卫（阶段 4b）**：「baseColor path 非空 + Handle 无效 → pending 不 prepare」在 [RenderSystem 模块](../../render_system/AGENTS.md) 的 PrepareAssetsSystem 侧，本模块只负责发加载与回填。

## 依赖关系
- 向上依赖：
  - [Core 模块](../../core/AGENTS.md)（JsonCursor、HashMap、String、Mat4/AABB）
  - [Asset 模块](../AGENTS.md)（`AssetServer`、`Assets<T>`、mesh/texture/material loader 与资产类型）
  - [ECS 模块](../../ecs/AGENTS.md)（World、GlobalTransform、System）
  - [RenderSystem 模块](../../render_system/AGENTS.md)（`MeshAssetRef`/`MaterialAssetRef`/`WorldAABB` 组件）
  - [Log 模块](../../log/AGENTS.md)（加载/回填日志，PRIVATE）
- 被依赖：
  - 游戏侧 Runtime 模块（`_game/source/runtime`，`RenderAssets` 持有 `SceneLoader` 实例，`GamePlugin` 注册回填系统并调用 `spawnCookedScene`）

## 技术债务

> 统一维护于 [TODO.md](../../../../TODO.md)。本模块相关条目包括：Asset/`SceneLoader::backfillMaterialTextures()` 轮询回填（过渡机制）、Asset/`MaterialAsset` 贴图路径与 Handle 双存。

## 测试
- 无单元测试（装配/搬迁类改动，与 3c/4b 先例一致）。验收方式：Debug 构建 + 既有全量测试绿 + 游戏实跑日志对账（405 实体 spawn、28 唯一材质、baseColor/normal/MR 贴图 loadAsync 零失败、零 pending/ERROR/WARN）。
