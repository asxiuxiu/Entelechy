# 渐进式 Render 管线补全计划 —— 以渲染 Sponza 为终点

> **生成日期**: 2026-08-04
> **修订**: 2026-08-05 —— 目标资产从 `_content/Classic-Sponza/`（Unity HDRP 工程）更换为 `_content/sponza/`（Intel NewSponza，glTF 2.0）。阶段 3/4 的导入链路相应重写：Unity YAML / .mat / PSD-TIF 转换全部不再需要。
> **修订**: 2026-08-05（二）—— `_game` 层分层审视：极简 JSON 解析器（`core/json/json_cursor.h` 的 `JsonCursor`）与 `AABB::transformed` 已提至 core，包围盒组件 `WorldAABB`/`RenderAABB` 已落户 `render_system`（游戏侧注册补丁拆除）；阶段 4 第 4 条相应改为「场景加载入口迁入引擎」。
> **依据**: [docs/RENDER_LAYER_PROGRESS.md](../docs/RENDER_LAYER_PROGRESS.md)（四大集成断裂 + 各节缺失项）
> **目标场景**: `_content/sponza/`（Intel NewSponza：`NewSponza_Main_glTF_003.gltf` + 140MB .bin + 137 张 PNG 纹理；glTF 2.0，115 meshes / 155 nodes / 28 materials，PBR metallic-roughness + normal map，顶点自带 TANGENT。附带的 929MB FBX 与 USD 变体均不使用）
> **原则**: 每个阶段都有**窗口里可见的改进**；优先修复阻断"画出来"的断裂，架构级升级（RenderGraph、延迟命令缓冲等）推迟到画面已成立之后。

---

## 现状一句话总结

四大集成断裂已全部修复，glTF 导入/cook 链路已打通：阶段 1-3 完成（2026-08-05），NewSponza 全量 405 primitive 以白模（法线着色）经 Extract → Cull → Queue → Execute 完整链路渲染，异步加载无 fallback 残留，视锥剔除实时生效，~60 fps（vsync 上限）。下一步是阶段 4（材质/纹理还原，全贴图 Sponza）。

## 断裂修复与阶段的对应关系

| 断裂 | 内容 | 修复阶段 |
|------|------|---------|
| #4 没有帧驱动层 | `RenderFrameRunner::runFrame()` 串联全流程 | 阶段 1 ✅ |
| #3 没有渲染消费端 | Execute 阶段消费 PhaseItems 发 draw call | 阶段 1 ✅ |
| #1 Handle 系统与 Render 组件不互通 | `MeshAssetRef`/`MaterialAssetRef` 改用 `Handle<T>` | 阶段 2 ✅ |
| #2 Prepare 阶段缺失 | asset → GPU 几何/纹理/材质参数解析 | 阶段 2 ✅ |

---

## 阶段总览

```
阶段 1  管线自己转起来 ✅     ✅    ECS 实体经完整管线画出多个立方体，自由相机漫游（2026-08-04 完成）
阶段 2  资源进管线 ✅        拆为 2a/2b/2c（见 plan/RENDER_PHASE2_PLAN.md），2026-08-05 完成：Handle 集成 + MeshAsset/TextureAsset/stb_image loader + PrepareAssetsSystem（fallback 热替换）；详细验收记录见子计划
阶段 3  看见 Sponza 骨架 ✅  拆为 3a/3b/3c（见 plan/RENDER_PHASE3_PLAN.md），2026-08-05 完成：.emesh 格式+Loader → cgltf cook 工具（405 primitive 零告警）→ spawn+白模渲染（405 实体异步加载无 fallback 残留，剔除生效，~60fps）；详细验收记录见子计划
阶段 4  还原材质与场景     glTF 场景图/材质还原，全贴图 Sponza
阶段 5  让它像样           光照、深度/法线正确性、天空、调试统计面板
阶段 6+ 架构补全           RenderGraph / 延迟命令缓冲 / TAI 材质 / BindGroup（按文档优先级推进）
```

---

## 阶段 1 —— 管线自己转起来 ✅ 已完成（2026-08-04，commit `3d33439`）

> **落地偏差**：帧驱动层实现为 `RenderFrameRunner`（`render_system/frame/`）而非 `RenderSystem::runFrame()`；Execute 消费端为 `RenderExecuteSystem`（`render_system/execute/`），阶段1 网格/材质由 `main.cpp.in` 手工注册（待阶段2 Prepare 替代，已记入 TODO.md）。深度测试已在材质的 `PipelineStateDesc` 中开启并由 `GLCommandList::bindPipeline` 应用。
> **验收**：Debug 构建通过，`EntelechyTests` 169 全绿；6×6 立方体阵列（含偶数行首列堆叠）经 Extract → Cull → Queue → Execute 完整链路绘制，遮挡正确；ImGui Render Stats 面板显示 draw calls / visible / culled。
> **后续修复（2026-08-05）**：demo 立方体索引缓冲顶面绕序错误（`3,2,6 / 3,6,7` 为顺时针，从上方看顶面被背面剔除，每个立方体只剩两个可见面），已在 `launch/templates/main.cpp.in` 修正为 `3,7,6 / 3,6,2` 并截图验证顶面恢复。

**可见成果**：窗口中不再由 `SimpleCubeRenderer` 硬编码绘制，而是主 World 里的若干 ECS 实体（立方体阵列、不同变换）经由 **Extract → Cull → Queue → Execute** 完整链路画出；WASD+鼠标自由飞行相机；视锥剔除实时生效（转身时剔除数变化）。

**主要工作**：

1. **帧驱动层**（断裂 #4）：新增 `RenderSystem::runFrame()`（或 `RenderFrameRunner`），编排：主 World `ExtractSchedule` → Cull → Queue → Execute → Present。挂在现有主循环（`build/generated/main.cpp` 由 `launch/generator.py` 生成，需改模板 `launch/templates/`）中 `app` 调度之后。
2. **Execute 消费端**（断裂 #3）：新增 `RenderExecuteSystem`，消费 `ViewBinnedPhases`/`ViewSortedPhases`，按 PhaseItem 绑定 mesh VBO/IBO + material uniform，走 `IRHICommandList` 发出 `drawIndexed()`。此阶段 material 仍是 Phase 1 简化 `Material`（立即 `glUniform*`），**不做 BindGroup 升级**。
3. **自由相机**：`Camera` 组件 + 飞行相机 System（`window/input` 已有输入队列），接入 `ExtractCameraSystem` 已支持的 `ExtractedView`。
4. **深度测试**：`PipelineStateDesc` 补齐 depth test/write，帧首 clear depth。没有它后面一切免谈。

**验证**：Debug 构建运行；立方体阵列正确遮挡；相机转向时 ImGui 面板显示剔除数变化；现有 `render_system` 测试全绿。

**对应章节**：5.4（帧驱动）、5.14（Render 阶段）。

---

## 阶段 2 —— 资源进管线 ✅ 已完成（2026-08-05，拆为 2a/2b/2c，验收记录见 [RENDER_PHASE2_PLAN.md](RENDER_PHASE2_PLAN.md)）

**可见成果**：场景中的物体（地面 + 几根柱子，可用程序化网格）**通过 AssetServer 异步加载**网格与 PNG 纹理后显示；日志可见加载状态流转；卸载后 GPU 资源经 Fence 延迟回收。

**主要工作**：

1. **Handle 集成**（断裂 #1）：`MeshAssetRef`/`MaterialAssetRef` 的裸 `u32 asset_id` 替换为 `asset/` 模块的 `Handle<MeshAsset>`/`Handle<MaterialAsset>`；`RenderMesh`/`RenderMaterial` 同步迁移。
2. **Prepare 阶段**（断裂 #2）：新增 `PrepareAssetsSystem`：将 Extract 来的 `Handle<T>` 经 `Assets<T>` 解析为 GPU 资源（未加载则触发 `loadAsync` 并用 fallback 材质/网格），产出 `PreparedMesh`（VBO/IBO RHIRef）与 `PreparedMaterial`（管线 + uniform 数据）。
3. **资产类型**：定义 `MeshAsset`（CPU 侧顶点/索引 + AABB）与 `TextureAsset`；实现对应 `IAssetLoader<T>`。纹理加载引入 `stb_image`（单头文件，先确认第三方库引入方式符合工程惯例）。

> ~~纹理转换工具~~ **已不需要**（2026-08-05 资产更换）：新 Sponza 的 137 张纹理全部是 PNG（BaseColor/Normal/Roughness/Metalness 及预打包的 RoughnessMetalness），stb_image 直接可读，无需 PSD/TIF 转换脚本。注意：部分纹理为 4K，6.3GB 总量主要来自 .max/.fbx 与高清 PNG，cook 时可按需降采样，但本计划不强制。

**验证**：Debug 构建；带贴图物体正确显示；热拔插（未加载→加载完成）过程中 fallback 粉色材质被正确替换；`test_gpu_resource_lifecycle` 等测试保持绿。

**对应章节**：5.8（Handle 集成）、5.14（Prepare）、5.9（加载状态机，先用现有简化流程）。

---

## 阶段 3 —— 看见 Sponza 骨架 ✅ 已完成（2026-08-05，拆为 3a/3b/3c：commits `9c09570` / `0a8bdaf` / `5bcf556`，验收记录见 [RENDER_PHASE3_PLAN.md](RENDER_PHASE3_PLAN.md)）

> **落地情况**：`.emesh` 二进制 mesh 格式 + `MeshAssetLoader`（3a）；C++ `mesh_cooker`（cgltf/Conan）对 NewSponza 实跑 405 primitive 全量 cook 零告警（3b）；游戏侧 `scene_loader` 读 `scene.json` 异步加载并 spawn 405 实体（烘焙 `GlobalTransform` + 世界 AABB），`MaterialAsset::shade_mode` 白模法线着色（3c）。
> **验收**：Debug 构建通过，`EntelechyTests` 186 全绿；实跑 405 mesh 全部上传、零 ERROR、无 fallback 残留；`Frame stats` 日志确认剔除实时生效（culled 随视角 17~388/405 变化）、~60 fps（vsync 上限，记录为优化基线）。**未截图**：引擎无帧读回/截图机制（已记 TODO.md），白模几何完整性由用户目视验收通过。

**可见成果**：`NewSponza_Main_glTF_003.gltf` 的**全部几何体以白模（单一灰色材质 + 法线着色）**渲染在窗口中，自由相机可在宫殿内漫游，剔除/排序正常工作。

**主要工作**：

1. **glTF 导入选型**：推荐 `cgltf`（单头文件 C 库、MIT、无构建负担，与 ufbx 同类但直接吃 glTF）。原计划的 FBX 路线（ufbx）放弃：新资产的 FBX 达 929MB 且不含场景/材质优势，glTF 一个文件同时给出几何、node 场景图和材质/纹理引用。**决策点：先在知识库检索/确认选型，再动手。**
2. **离线 cook**：Python 或 C++ 小工具将 glTF（经 cgltf 或 Python 侧解析）→ 引擎自有 mesh 格式（顶点流交错 + 索引 + 子网格/primitive + 每 primitive 材质名/索引），输出到 `_content/sponza/cooked/meshes/`。离线 cook 而非运行时直接吃 glTF，保持运行时加载简单。glTF 是 Y-up 右手系、与引擎一致，但**索引/顶点需注意 accessor 的 componentType 与 interleaved 布局**。
3. **primitive/node → 实体**：glTF 的 155 个 node 自带 TRS 层级，115 个 mesh 共数百个 primitive；每个 primitive 成为一个可渲染实体（或一个实体的 sub-mesh 数组——按当前 `RenderMesh` 结构选择更简单者），node 世界变换在 cook 时或 spawn 时烘焙。每帧数百 draw call，现有 CPU 暴力视锥剔除足够。
4. **法线可视化着色**：白模阶段用法线当颜色（或简单 N·L），避免"全黑看不出几何是否正确"。glTF 顶点自带 NORMAL/TANGENT，无需自行生成。

**验证**：Debug 构建；Sponza 几何完整（拱门、柱子、帘幕可辨认）；相机穿模检查包围盒正确性；帧率记录为后续优化基线。

**对应章节**：5.9（加载链路实战）、5.5（暴力剔除在数百实体下的可行性验证）。

---

## 阶段 4 —— 还原材质与场景

**可见成果**：**全贴图的 NewSponza**——砖墙、拱门、地砖、旗帜各就各位，布局与 glTF 场景图一致（node 层级 TRS 即场景，无需外部场景文件）。

**主要工作**：

1. **场景还原来源切换**：原计划解析 `Sponza.unity`（Unity YAML + prefab 嵌套，原最大风险点）**整体取消**。glTF 的 node 树就是场景：cook 时导出场景 JSON（实体列表 = {cooked mesh/primitive 引用, world transform, 材质引用}），或阶段 3 已直接 spawn 的话本阶段只需补材质引用。
2. **材质转换**：glTF `pbrMetallicRoughness` → 引擎材质 JSON：baseColorTexture（PNG 路径）、normalTexture、metallicRoughnessTexture（G=roughness、B=metallic 的打包约定需在着色器侧对应解包）、baseColorFactor/metallicFactor/roughnessFactor。28 个材质的映射规则写死在转换器里即可，不做通用系统。
3. **材质贴图进 Prepare**：`MaterialAsset` 增加纹理 `Handle<TextureAsset>`，Prepare 阶段绑定贴图；阶段 2 的 fallback 机制覆盖加载中状态。
4. **场景加载入口迁入引擎**：阶段 3c 已在游戏侧落地最小形态（`_game/source/runtime/scene_loader.cpp` 的 `spawnCookedScene`），但 2026-08-05 分层审视确认其归属引擎——`scene.json` 是引擎工具 `mesh_cooker` 的自有格式，格式归引擎、解析归游戏层不对称，第二个游戏需原样重写（已记 TODO.md）。本阶段补材质引用时顺势把 loader 迁入引擎（`asset/` 或独立 scene 模块），游戏侧只传场景路径；cooker 的 `scene.json` 同步补材质字段。现有基础设施可直接复用：解析用 core `JsonCursor`（`core/json/json_cursor.h`，3c 后已共享），包围盒组件用引擎侧 `WorldAABB`/`RenderAABB`（游戏侧注册补丁已拆除）。

**验证**：Debug 构建；与资产包内官方渲染图（`_content/sponza/Render_Main_*.png`）对比布局一致；纹理无错位（glTF UV 原点在左上，贴图采样时 V 翻转问题在此阶段暴露并修复）；全部纹理加载完成后无 fallback 残留。

**对应章节**：5.11（MaterialAsset 雏形，但**不上 TAI 三层**）、5.13（贴图绑定）。

---

## 阶段 5 —— 让它像样

**可见成果**：有方向光（阳光）照射的 Sponza，明暗正确、有法线贴图效果；天空色/简易天空盒；ImGui 面板显示 FPS、draw call 数、剔除前后实体数、GPU 内存。

**主要工作**：

1. **光照**：方向光 + 环境项（材质转换器已导出 metallic/roughness，直接上 GGX 更值）；光源做成 ECS 组件 + Extract。glTF 声明了 `KHR_lights_punctual` 扩展但主文件 lights 数组为空，光照参数需自配（参照官方渲染图的日光角度）。
2. **法线贴图**：glTF 顶点自带 TANGENT，cook 时直接随顶点流写入即可，无需生成或屏幕空间近似。
3. **天空**：先用清屏渐变/纯色；资产包不含天空盒，如需天空盒另行准备。
4. **调试面板**：ImGui 显示渲染统计（draw calls、culled/total、PSO 命中、显存追踪——`queryMemoryInfo`/`getTrackedMemoryUsage` 已有）。

**验证**：Debug 构建；光照方向与官方渲染图观感一致；统计面板数字合理。

**对应章节**：5.13（按更新频率分层的 View/Light 参数可先以简化形式落地）。

---

## 阶段 6+ —— 架构补全（画面已成立，按文档优先级偿还债务）

画面目标达成后，回头按 [RENDER_LAYER_PROGRESS.md](../docs/RENDER_LAYER_PROGRESS.md)「建议的下一步优先级」补架构，每项独立完成、可随时停下：

1. **5.7 RenderGraph 最小实现**（<500 行编译器：拓扑排序 + 死 Pass 剔除 + Barrier 插入）——为阴影 pass、后处理（5.18/5.19）铺路。
2. **5.1 延迟命令缓冲**——`RenderCommandBuffer` + `LinearAllocator`，修复立即执行偏离，为 D3D12 过渡铺路。
3. **5.11/5.12 材质 TAI 三层 + 着色器变体/缓存**——替换 Phase 1 简化 Material 临时代码。
4. **5.13 BindGroup/UBO 分层**——解决 10+ 材质时 `glUniform*` 性能问题。
5. **5.3b PSO 异步编译**、**5.5 BVH 空间加速**（Sponza 数百实体可暂缓，场景变大后必做）。
6. **5.15/5.16/5.17** 2D/字体/UI——阶段 6 自研 UI 框架的前置。

---

## 跨阶段注意事项

- **每阶段验收都跑 Debug 构建**（`python scripts/build/build.py --debug --build`），并保证既有单元测试不红。
- **文档同步**：每阶段结束后更新 `docs/RENDER_LAYER_PROGRESS.md` 对应章节的"代码现状"与完成度条，以及相关模块 `AGENTS.md`。
- **遇阻即停**：glTF 解析/cook（accessor 布局、交错顶点流、V 翻转）、metallicRoughness 打包纹理解包两处是已知风险点，卡住时按 AGENTS.md 规则停下汇报，不自行换方案绕过。原三大高风险点（Unity YAML 解析、FBX 导入、HDRP 参数映射）随资产更换已消解。
- **不做的事**：本计划不追求性能优化（Bindless/GPU-Driven/HZB）、不追求渲染品质（阴影/TAA/Bloom）、不迁移 D3D12——这些属于阶段 6+ 或更后期。
- 每阶段完成后按 AGENTS.md「事后审视」规则，把偏差与新技术债务记入 `TODO.md`。
