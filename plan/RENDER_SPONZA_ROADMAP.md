# 渐进式 Render 管线补全计划 —— 以渲染 Sponza 为终点

> **生成日期**: 2026-08-04
> **修订**: 2026-08-05 —— 目标资产从 `_content/Classic-Sponza/`（Unity HDRP 工程）更换为 `_content/sponza/`（Intel NewSponza，glTF 2.0）。阶段 3/4 的导入链路相应重写：Unity YAML / .mat / PSD-TIF 转换全部不再需要。
> **修订**: 2026-08-05（二）—— `_game` 层分层审视：极简 JSON 解析器（`core/json/json_cursor.h` 的 `JsonCursor`）与 `AABB::transformed` 已提至 core，包围盒组件 `WorldAABB`/`RenderAABB` 已落户 `render_system`（游戏侧注册补丁拆除）；阶段 4 第 4 条相应改为「场景加载入口迁入引擎」。
> **依据**: [docs/RENDER_LAYER_PROGRESS.md](../docs/RENDER_LAYER_PROGRESS.md)（四大集成断裂 + 各节缺失项）
> **目标场景**: `_content/sponza/`（Intel NewSponza：`NewSponza_Main_glTF_003.gltf` + 140MB .bin + 137 张 PNG 纹理；glTF 2.0，115 meshes / 155 nodes / 28 materials，PBR metallic-roughness + normal map，顶点自带 TANGENT。附带的 929MB FBX 与 USD 变体均不使用）
> **原则**: 每个阶段都有**窗口里可见的改进**；优先修复阻断"画出来"的断裂，架构级升级（RenderGraph、延迟命令缓冲等）推迟到画面已成立之后。

---

## 现状一句话总结

四大集成断裂已全部修复，glTF 导入/cook 链路已打通：阶段 1-5 完成（2026-08-06），NewSponza 全量 405 primitive 经 Extract → Cull → Queue → Execute 完整链路渲染——方向光 lit PBR（GGX）、法线/MR 贴图采样、天空渐变、调试统计面板（FPS/draw calls/剔除/PSO 缓存/显存）全部落地，场景加载入口已归引擎（`asset/scene/`），异步加载无 fallback 残留，视锥剔除实时生效，~60 fps（vsync 上限）。阶段 6a RHI 接口审计 + 6b 延迟命令缓冲已完成（2026-08-07），GL 后端已从立即执行切换为录制+回放模式。阶段 6c 着色器编译工具链已完成（2026-08-07）：HLSL-first + DXC + SPIRV-Cross 管线落地，3 组 shader 移植为 HLSL SM 6.0，离线编译产出 DXIL/SPIR-V/GLSL 三格式，运行时从预编译字节码加载。阶段 6d D3D12 后端已完成（2026-08-07）：Sponza 在 DX12 下渲染与 GL 功能等价（~114 fps，剔除统计一致），`--backend=d3d12` 选择后端，默认仍 OpenGL。阶段 6e UBO/CBV 统一绑定层已完成（2026-08-08）：ConstantBufferRing + BindGroup/反射元数据驱动，GL 真 UBO / D3D12 root CBV 统一绑定路径，SPIRV-Cross 展平命名债务彻底消除，双后端画面与 6d 一致。下一步是 6f（Vulkan 预留）/ 6g（PSO 异步编译）。

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
阶段 4  还原材质与场景 ✅  拆为 4a/4b/4c（见 plan/RENDER_PHASE4_PLAN.md），2026-08-06 完成：cooker 材质导出（28 个 .emat）+ MaterialAssetLoader → baseColor 贴图上屏（V 翻转/alpha/doubleSided 落地）→ scene_loader 迁引擎 asset/scene + normal/MR 贴图加载落位（只加载不采样，D4）；详细验收记录见子计划
阶段 5  让它像样 ✅        拆为 5a/5b/5c（见 plan/RENDER_PHASE5_PLAN.md），2026-08-06 完成：方向光+lit PBR shader → 法线/MR 贴图采样（含 mipmap 修复）→ 天空渐变+调试统计面板（PSO 缓存顺势接线）；详细验收记录见子计划
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

## 阶段 4 —— 还原材质与场景 ✅ 已完成（2026-08-06，拆为 4a/4b/4c，验收记录见 [RENDER_PHASE4_PLAN.md](RENDER_PHASE4_PLAN.md)）

**可见成果**：**全贴图的 NewSponza**——砖墙、拱门、地砖、旗帜各就各位，布局与 glTF 场景图一致（node 层级 TRS 即场景，无需外部场景文件）。

**主要工作**：

1. **场景还原来源切换**：原计划解析 `Sponza.unity`（Unity YAML + prefab 嵌套，原最大风险点）**整体取消**。glTF 的 node 树就是场景：cook 时导出场景 JSON（实体列表 = {cooked mesh/primitive 引用, world transform, 材质引用}），或阶段 3 已直接 spawn 的话本阶段只需补材质引用。
2. **材质转换**：glTF `pbrMetallicRoughness` → 引擎材质 JSON：baseColorTexture（PNG 路径）、normalTexture、metallicRoughnessTexture（G=roughness、B=metallic 的打包约定需在着色器侧对应解包）、baseColorFactor/metallicFactor/roughnessFactor。28 个材质的映射规则写死在转换器里即可，不做通用系统。
3. **材质贴图进 Prepare**：`MaterialAsset` 增加纹理 `Handle<TextureAsset>`，Prepare 阶段绑定贴图；阶段 2 的 fallback 机制覆盖加载中状态。
4. **场景加载入口迁入引擎**：阶段 3c 已在游戏侧落地最小形态（`_game/source/runtime/scene_loader.cpp` 的 `spawnCookedScene`），但 2026-08-05 分层审视确认其归属引擎——`scene.json` 是引擎工具 `mesh_cooker` 的自有格式，格式归引擎、解析归游戏层不对称，第二个游戏需原样重写（已记 TODO.md）。本阶段补材质引用时顺势把 loader 迁入引擎（`asset/` 或独立 scene 模块），游戏侧只传场景路径；cooker 的 `scene.json` 同步补材质字段。现有基础设施可直接复用：解析用 core `JsonCursor`（`core/json/json_cursor.h`，3c 后已共享），包围盒组件用引擎侧 `WorldAABB`/`RenderAABB`（游戏侧注册补丁已拆除）。

**验证**：Debug 构建；与资产包内官方渲染图（`_content/sponza/Render_Main_*.png`）对比布局一致；纹理无错位（glTF UV 原点在左上，贴图采样时 V 翻转问题在此阶段暴露并修复）；全部纹理加载完成后无 fallback 残留。

**对应章节**：5.11（MaterialAsset 雏形，但**不上 TAI 三层**）、5.13（贴图绑定）。

---

## 阶段 5 —— 让它像样 ✅ 已完成（2026-08-06）

> **已拆分子计划**（2026-08-06）：见 [RENDER_PHASE5_PLAN.md](RENDER_PHASE5_PLAN.md)，拆为 5a（方向光+lit PBR shader）/ 5b（法线贴图+MR 采样）/ 5c（天空渐变+调试统计面板）。三个子步均已落地：5a/5b 目视验收见子计划落地记录，5c 天空观感用户已目视确认。

**可见成果**：有方向光（阳光）照射的 Sponza，明暗正确、有法线贴图效果；天空色/简易天空盒；ImGui 面板显示 FPS、draw call 数、剔除前后实体数、GPU 内存。

**主要工作**：

1. **光照**：方向光 + 环境项（材质转换器已导出 metallic/roughness，直接上 GGX 更值）；光源做成 ECS 组件 + Extract。glTF 声明了 `KHR_lights_punctual` 扩展但主文件 lights 数组为空，光照参数需自配（参照官方渲染图的日光角度）。
2. **法线贴图**：glTF 顶点自带 TANGENT，cook 时直接随顶点流写入即可，无需生成或屏幕空间近似。
3. **天空**：先用清屏渐变/纯色；资产包不含天空盒，如需天空盒另行准备。
4. **调试面板**：ImGui 显示渲染统计（draw calls、culled/total、PSO 命中、显存追踪——`queryMemoryInfo`/`getTrackedMemoryUsage` 已有）。

**验证**：Debug 构建；光照方向与官方渲染图观感一致；统计面板数字合理。

**对应章节**：5.13（按更新频率分层的 View/Light 参数可先以简化形式落地）。

---

## 阶段 6 —— 多后端架构基础（以 DX12 为默认后端，Vulkan 预留）

> **修订**: 2026-08-07 —— 原"架构补全"拆分为阶段 6（多后端架构基础）与阶段 7+（高级特性）。核心目标：将渲染管线从 OpenGL-only 重构为真正的多后端架构，DX12 作为默认生产后端，OpenGL 保留为调试/兼容后端，Vulkan 接口预留。每项子任务独立可验收、可随时停下。
> **原则**: 先让 DX12 跑通现有 Sponza 画面（功能等价），再做架构优化；避免"为了抽象而抽象"的过度设计。

### 6a. RHI 接口审计与多后端对齐 ✅ 已完成（2026-08-07）

**目标**: 审视并调整 `IRHIDevice` / `IRHICommandList` / `IRenderBackend` 接口，使其真正适配 D3D12/Vulkan 语义，而非仅面向 OpenGL 的薄包装。

**主要工作**:

1. **`IRenderBackend` ↔ `IRHIDevice` 合并或重定义**：当前两者分离（swapchain/frame 管理 vs 资源工厂+命令提交），但 D3D12/Vulkan 中 swapchain 与 device/queue 紧耦合。评估两种方案：(A) 合并为单一 `IRHIDevice`；(B) 保留分层但让 `IRenderBackend` 持有 `IRHIDevice` 引用并由设备创建。**决策点：先在知识库检索 UE/Bevy/FNA 的多后端 RHI 分层实践，再选定方案。**
2. **`BarrierDesc` 扩展**：当前仅 4 种状态（Undefined/Common/RenderTarget/ShaderResource），D3D12 需要 UAV、Copy Source/Dest、Resolve、Present 等完整 `D3D12_RESOURCE_STATES` 映射。重新设计为位掩码或枚举集合，同时保持 GL 后端的 no-op 语义。
3. **`IRHICommandList::setUniform*` 废弃路径规划**：当前 per-draw uniform 上传是纯 GL 语义。D3D12 走 Root Constants / CBV Descriptor Table，Vulkan 走 Push Constants / Descriptor Set。制定统一替代方案：引入 `bindConstantBuffer(binding, offset, size)` + `setPushConstants(offset, data)` 接口，GL 后端内部仍翻译为 `glUniform*`（UBO 过渡期兼容）。
4. **`createShader()` 接口调整**：当前接受 GLSL 源码字符串。D3D12 需要 DXIL 字节码，Vulkan 需要 SPIR-V。改为接受 `ShaderBytecode { stage, format(SPIRV|DXIL|GLSL), data, entryPoint }` 结构体；GL 后端继续吃 GLSL，D3D12/VK 后端吃编译产物。离线编译工具链在 6c 落地。
5. **Fence/Sync 模型泛化**：当前 `GLsync` 单队列模型。扩展 `IRHIFence` 为独立资源类型（`createFence()` / `signalFence()` / `waitFence()` / `getCompletedValue()`），支持多队列同步。GL 后端用 `GL_SYNC_GPU_COMMANDS_COMPLETE` 模拟。
6. **`RenderBackendType` 枚举扩展**：新增 `D3D12`、`Vulkan` 值；设备工厂函数 `createRHIDevice(RenderBackendType)` 集中到 `rhi_device_factory.h`。

**验证**: 接口改动后 GL 后端编译通过 + 既有测试全绿 + Sponza 画面无回归。接口变更记入 `docs/RENDER_LAYER_PROGRESS.md` 5.1 节。

> **落地记录（2026-08-07）**：六项子任务全部完成。(1) 行业调研确认 UE/SDL GPU/bgfx 均为 device+backend 合一模式，选择方案 A 完全合并——删除 `IRenderBackend`/`OpenGLBackend`，surface 生命周期 7 个方法并入 `IRHIDevice`，设备由 main() 持有、RenderExecuteSystem 借用。(2) `ResourceState` 位掩码 12 种状态 + resource-referencing `BarrierDesc`（含 mipLevel/arrayLayer），旧 `ResourceBarrierType` 移除。(3) `bindConstantBuffer()` + `setPushConstants()` 新增到 `IRHICommandList`，GL 实现 no-op；现有 `setUniform*` 保留待 6e 迁移。(4) `createShader()` 改接受 `ShaderBytecode{stage, format, data, size, entryPoint}`，GL 后端校验 format==GLSL。(5) `IRHIFence` 提取为 GPUResource 子类（signal/wait/getCompletedValue/isSignaled），`GLFence` 底层 GLsync，`createFence()` 工厂方法就位。(6) `RenderBackendType` 扩展 D3D12/Vulkan，`createRHIDevice()` 工厂函数建立。Debug 构建通过，223 测试全绿。详细变更记录见 `docs/RENDER_LAYER_PROGRESS.md` 5.1 节。

**对应章节**: 5.1（RHI 抽象层）、5.3（资源生命周期——Fence 泛化）

---

### 6b. 延迟命令缓冲（Deferred Command Buffer）⭐ ✅ 已完成（2026-08-07）

**目标**: 将 `GLCommandList` 从立即执行模式切换为延迟录制+回放模式，使同一套 `IRHICommandList` 调用在 GL/D3D12/VK 下语义一致。这是多后端架构的核心前提——D3D12/VK 天然就是延迟录制，如果上层仍是立即模式则无法利用并行 command list。

**主要工作**:

1. **`RenderCommandBuffer` + `LinearAllocator`**：每帧分配 4–16 MB 线性内存，所有 `IRHICommandList` 调用序列化为 `(RenderCmdHeader + payload)` 写入 buffer。命令枚举覆盖现有全部操作（BeginRenderPass / EndRenderPass / BindPipeline / BindVertexBuffer / BindIndexBuffer / BindTexture / SetUniform* / DrawIndexed / ResourceBarrier / SetViewport / PushDebugGroup / PopDebugGroup）。
2. **`GLCommandTranslator`**：遍历 `RenderCommandBuffer` 中的命令序列，逐条翻译为 GL 调用。替换当前 `GLCommandList` 的直接执行逻辑。状态缓存（bound program/VAO/EBO/uniform location）迁入 translator。
3. **`RenderExecuteSystem` 适配**：不再直接调 `cmdList->drawIndexed()`，而是向 `RenderCommandBuffer` 录制命令。帧末由 `RenderFrameRunner` 统一 submit → translate → execute。
4. **GL 后端行为等价验证**：录制+回放后的 GL 调用序列必须与改造前完全一致（可通过 debug marker 或 API trace 对比）。

**验证**: Debug 构建通过；Sponza 画面、帧率、剔除统计与改造前一致；单元测试全绿；新增命令缓冲序列化/反序列化测试。

**对应章节**: 5.1（延迟命令缓冲）、5.2（多线程命令录制 Phase 1）

> **落地记录（2026-08-07）**：四项子任务全部完成。(1) `RenderCommandBuffer` 实现为连续内存 bump allocator（4 MB 默认容量），`RenderCommandType` 枚举覆盖全部 24 种 IRHICommandList 操作，payload 结构体均为 POD/可平凡拷贝，变长数据（debug string / push constants / barrier 数组）以 inline trailing data 方式嵌入 buffer。(2) `DeferredCommandList` 实现 `IRHICommandList` 接口，每个方法分配 payload + memcpy 参数到 buffer，零 GL 调用。(3) `GLCommandTranslator` 遍历 buffer 逐条翻译为 GL 调用，状态缓存（bound program/VAO/EBO/uniform location cache）从旧 `GLCommandList` 完整迁入。(4) `GLRHIDevice` 删除 `GLCommandList` 类及全部立即执行实现，改用 pimpl `DeferredState` 持有 buffer/recorder/translator；`createCommandList()` 重置 buffer 返回 recorder，`submit()` 调用 translator.execute()。`RenderExecuteSystem` / `RenderFrameRunner` / `Material` 零改动——完全通过 `IRHICommandList` 接口透明切换。Debug 构建通过，234 测试全绿（原 223 + 新增 11 个命令缓冲单元测试）。详细变更记录见 `docs/RENDER_LAYER_PROGRESS.md` 5.1 节。

---

### 6c. 跨平台着色器编译工具链 ⭐ ✅ 已完成（2026-08-07）

**目标**: 建立统一的着色器编译管线，从单一 GLSL/HLSL 源码产出 GLSL（GL 后端）、SPIR-V（Vulkan）、DXIL（D3D12）三种目标格式。这是 D3D12 后端落地的硬性前置——没有字节码就无法创建 D3D12 PSO。

**主要工作**:

1. **编译器选型**：推荐 **DXC**（Microsoft DirectX Shader Compiler）作为主编译器——原生 HLSL→DXIL，配合 SPIR-V 输出能力（`-spirv`）可同时覆盖 D3D12+Vulkan。GL 后端通过 **SPIRV-Cross** 将 SPIR-V 反编译回 GLSL（或直接维护 GLSL 变体，视复杂度决定）。备选：glslang + SPIRV-Cross 全链路。**决策点：先在知识库确认 DXC 的 Conan/CMake 集成可行性与 SPIR-V 输出成熟度。**
2. **着色器语言选择**：评估 HLSL-first vs GLSL-first vs 自定义 IR。考虑到 D3D12 为主力后端且 DXC 对 HLSL 支持最完善，**倾向 HLSL-first + SPIR-V 中间表示 + SPIRV-Cross 降级 GLSL**。需评估现有 PBR lit shader（约 200 行 GLSL）移植到 HLSL SM 6.0 的工作量。
3. **离线编译工具**：`_engine/tools/shader_compiler/`，输入 `.hlsl` / `.glsl` + 编译配置 JSON（target profiles、include paths、macro defines），输出 `{stage}.dxil` / `{stage}.spv` / `{stage}.glsl` + 反射元数据（CBV/SRV/UAV binding、push constant layout）。Conan 引入 DXC/SPIRV-Cross。
4. **运行时加载**：`IRHIDevice::createShader()` 改为读取预编译字节码文件（6a 已调整接口）；开发期可选实时重编译（文件 watch + 热重载，属 5.10 范畴，本阶段不做）。
5. **现有 shader 迁移**：将当前内嵌在 C++ 代码中的 GLSL 字符串提取为独立 `.hlsl` 文件，经工具链编译验证三后端产物正确。

**验证**: 工具链可从 HLSL 源码产出 DXIL + SPIR-V + GLSL 三份产物；GL 后端经 SPIRV-Cross 降级路径渲染 Sponza 画面与改造前一致；DXIL 产物可通过 `dxc -disasm` 反汇编验证。

**对应章节**: 5.12（着色器变体与编译缓存）、5.1（RHI——shader 接口）

> **落地记录（2026-08-07）**：五项子任务全部完成。(1) 编译器选型确认 DXC v1.9.2607（GitHub Releases 预编译包，含 SPIR-V CodeGen），Conan 无 DXC 配方故采用手动预编译 + `cmake/FindDXC.cmake`；SPIRV-Cross 经 Conan 引入 `spirv-cross/1.4.350.0`（静态链接 C API）。(2) 着色器语言选定 HLSL-first SM 6.0；3 组内嵌 GLSL（PBR lit ~130 行、sky gradient ~25 行、simple cube ~15 行）移植为 6 个独立 `.hlsl` 文件于 `_engine/shaders/`，uniforms 重组为 cbuffer（PerFrame/PerMaterial/PerDraw），纹理改用 Texture2D+SamplerState 分离绑定。(3) `_engine/tools/shader_compiler/` 实现为 EXECUTABLE 模块：`DxcCompiler` 封装 IDxcCompiler3（HLSL→DXIL + HLSL→SPIR-V），`SpirvCrossCompiler` 封装 spvc C API（SPIR-V→GLSL 330，UBO 展平 + combined sampler 构建），CLI main 解析 `shaders.json` 批量编译。构建时 CMake post-build 自动运行，产物输出至 `build/bin/{Config}/shaders/`。(4) `Material::initFromBytecode()` 新增重载接受字节码文件路径 + ShaderBytecodeFormat，内部读文件→构造 ShaderBytecode→调用 createShader()；3 个调用方（PrepareAssetsSystem/RenderExecuteSystem/SimpleCubeRenderer）从内嵌字符串切换为 bytecode 加载，uniform 参数表适配 SPIRV-Cross 展平后的 vec4 数组命名（type_PerFrame[N]/type_PerDraw[N]/type_PerMaterial[N]）。(5) 6 个 entry point 全部编译成功（6 DXIL + 6 SPIR-V + 6 GLSL），DXIL 经 `dxc -dumpbin` 验证有效。Debug 构建通过，234 测试全绿。**已知限制**：SPIRV-Cross 展平后 uniform 名为数组索引形式（非原始 HLSL 成员名），材质参数表需手动匹配，将在 6e UBO/CBV 统一绑定层中彻底解决。~~combined sampler 名为 SPIRV-Cross 自动生成（_221/_223/_225），需在材质初始化时硬编码对应~~（2026-08-07 修复：该自动 ID 按首次采样顺序分配而非声明顺序，曾导致 normal/MR 贴图绑反；现 `SpirvCrossCompiler` 在 `build_combined_image_samplers` 后将合并采样器重命名回原始 HLSL 贴图名，C++ 按 `uBaseColorTex`/`uNormalTex`/`uMRTex` 绑定；sky PS cbuffer 同步改名 `PerFramePS`，消除 VS/PS 展平 uniform 同名冲突）。**6c 渲染回归追加修复（2026-08-07，截图机制定位）**：SPIRV-Cross GLSL 330 输出不带 stage 接口 `layout(location)`，VS 输出 `out_var_*` 与 FS 输入 `in_var_*` 按名链接恒失配，所有 FS 输入读零（贴图/法线/天空渐变全部失效、画面只剩角点 texel 平色）——GLSL 目标版本升至 410 后产出显式 location，截图验证全贴图恢复。详细变更记录见 `docs/RENDER_LAYER_PROGRESS.md` 5.12 节。

---

### 6d. D3D12 后端最小实现 ⭐⭐ ✅ 已完成（2026-08-07）

**目标**: 实现 D3D12 后端，使 Sponza 场景在 DX12 下可渲染，画面与 GL 后端功能等价。这是阶段 6 的核心交付物。

**依赖**: 6a（接口对齐）、6b（延迟命令缓冲）、6c（着色器编译）全部完成后开始。

**主要工作**:

1. **`D3D12RHIDevice`**：实现 `IRHIDevice` 全部接口。Device + CommandQueue + SwapChain 初始化（DXGI 1.4）、Fence 管理（`ID3D12Fence` + 单调递增 fence value）、资源创建（`CreateCommittedResource`）、PSO 创建（`CreateGraphicsPipelineState`）、内存追踪（`IDXGIAdapter3::QueryVideoMemoryInfo`）。
2. **`D3D12CommandList`**：实现 `IRHICommandList`，底层录制到 `ID3D12GraphicsCommandList`。若 6b 已完成延迟命令缓冲，则此步为 `D3D12CommandTranslator`（遍历 `RenderCommandBuffer` → 翻译为 D3D12 API 调用）；否则直接在 `D3D12CommandList` 中录制。**优先走延迟缓冲路径。**
3. **Root Signature 设计**：按更新频率分层——Slot 0: View CBV（per-frame）、Slot 1: Light CBV（per-frame）、Slot 2: Material CBV/SRV（per-material）、Slot 3: Object CBV（per-draw push constants / dynamic CBV）。Descriptor Heap 管理（CBV/SRV/UAV heap + sampler heap）。
4. **`D3D12Backend`**：实现 `IRenderBackend`（或与 `D3D12RHIDevice` 合并后的等价接口），管理 SwapChain present、frame resource ring buffer（2–3 帧 in-flight）、command allocator 轮换。
5. **资源屏障自动插入**：基于 `BarrierDesc`（6a 扩展后）翻译为 `D3D12_RESOURCE_BARRIER`。初期可在每次 bind/draw 前保守插入屏障，后续由 RenderGraph 优化。
6. **后端选择机制**：启动时根据配置 / 命令行参数 / 硬件检测选择后端；GL 与 D3D12 共存于同一二进制。

**验证**: Debug 构建通过；D3D12 后端启动 Sponza 场景，画面与 GL 后端目视一致（光照/贴图/天空/剔除）；PIX capture 验证命令流合理；帧率 ≥ GL 后端 80%（初期允许开销）。新增 D3D12 后端冒烟测试。

**对应章节**: 5.1（D3D12 后端）、5.3（GPU 资源生命周期——D3D12 Fence/Heap）、5.13（BindGroup/CBV 分层）

> **落地记录（2026-08-07）**：六项子任务全部完成。(1) `D3D12RHIDevice`（`render/public/rhi/d3d12_rhi_device.h` + `render/private/rhi/d3d12_rhi_device.cpp`）实现 `IRHIDevice` 全接口：DXGI 1.6 高性能适配器选择、flip-model swapchain（2 帧 RGBA8）+ 自有 D32 深度缓冲、`CreateCommittedResource` 资源创建（default heap + 专用 upload list 同步上传）、PSO 经 `PSOManager` 缓存、`IDXGIAdapter3::QueryVideoMemoryInfo` 显存查询。(2) 走延迟缓冲路径：`D3D12CommandTranslator` 遍历 `RenderCommandBuffer` 翻译为 D3D12 调用，上层 `RenderExecuteSystem`/`Material` 零改动。(3) Root Signature 全局一份：CBV b0..b2 按 stage visibility 拆分（slot 0-5）+ SRV 表 t0..t2（slot 6）+ 3 个静态 sampler；cbuffer 布局用 **DXC 容器反射**（`IDxcContainerReflection`，旧 `D3DReflect` 不支持 DXIL——首次实跑踩坑后改）在 PSO 创建时反射获得；翻译器把 `setUniform*` 的展平名（`type_PerDraw[N]`）解析回 (cbuffer, vec4 索引)，CPU staging 累积后 draw 时上传 per-frame upload ring（8MB）并绑 root CBV；纹理 SRV 创建在 256 槽持久 CPU heap（free-list 回收），draw 时 copy 进 4096 槽 per-frame shader-visible heap。(4) 6a 已完成 device/backend 合并，`D3D12RHIDevice` 直接管 swapchain present + 2 帧 in-flight allocator 轮换（fence 按 slot 等待）。(5) `BarrierDesc` → `D3D12_RESOURCE_BARRIER` 翻译落地（含 backbuffer PRESENT↔RENDER_TARGET、纹理 COPY_DEST→PS_RESOURCE）。(6) 后端选择：`--backend=d3d12` 或 `ENTELECHY_BACKEND` 环境变量，默认仍 OpenGL；`main.cpp.in` 按后端分支（GL 传 GLFWwindow*、D3D12 传 HWND），**D3D12 模式暂跳过 ImGui**（无 DX12 ImGui 后端，统计仍有每秒日志）。配套变更：`PipelineStateDesc` 增加顶点布局字段（D3D12 PSO 必需，GL 忽略；attribute location→HLSL 语义名映射在 D3D12 后端内），着色器路径按后端选扩展名（`shaderFileExtensionForBackend`）。**跨后端语义确认**：`Mat4::perspective` 本就是 z∈[0,1] D3D 风格无需修正；cooked UV 的 V 翻转是为 GL 底左原点服务的几何侧约定，D3D12 上传纹理时翻转像素行序等价。**验收**：Debug 构建通过，239 测试全绿（含 5 个新增 D3D12 冒烟：设备/buffer/纹理/fence/显存）；D3D12 实跑 Sponza 零错误日志，~114 fps（vs GL vsync 上限 60），剔除统计一致（285 visible / 120 culled）；截图验证几何/光照/贴图/天空与 GL 一致。**已知限制（记 TODO）**：纹理只建 mip 0（无运行时 mip 生成，静态 sampler clamp LOD=0，远景略 aliasing）；每纹理上传为同步 execute+wait（加载期偏慢）；PIX 调试标注未接（debug marker 丢弃）；D3D12 模式无 ImGui 叠加层。

---

### 6e. UBO/CBV 统一绑定层 ⭐ ✅ 已完成（2026-08-08）

**目标**: 替换当前 per-draw `glUniform*` 立即上传模式，建立跨后端的 Constant Buffer / Uniform Buffer Object 绑定抽象。这既是 GL 后端的性能修复（10+ 材质时 `glUniform*` 瓶颈），也是 D3D12 Root Signature / Vulkan Descriptor Set 的统一表达。**同时消除 6c SPIRV-Cross 降级路径遗留的 uniform 命名脆弱性问题。**

**依赖**: 可与 6d 并行推进（GL 侧先行），但需在 6d 完成前合入以统一绑定路径。

**主要工作**:

1. **`ConstantBufferRing`**：per-frame ring buffer（mapped persistent buffer），按 draw 分配对齐块，写入 view/light/material/object 常量。GL 后端为 UBO（`GL_UNIFORM_BUFFER`），D3D12 为 upload heap CBV，VK 为 host-visible buffer。
2. **`BindGroupLayout` / `BindGroup`**：声明式绑定描述——layout 定义 binding 槽位（type + stage visibility + count），BindGroup 实例绑定具体资源（buffer view + texture view + sampler）。GL 后端翻译为 `glBindBufferBase` + `glBindTextureUnit`；D3D12 翻译为 descriptor table set；VK 翻译为 `vkUpdateDescriptorSets`。
3. **`Material::bind()` 重写**：不再逐参数 `setUniform*`，改为填充 `ConstantBufferRing` 块 + 绑定 `BindGroup`。CPU uniform blob 保留作为 staging 区，bind 时一次性 memcpy 到 ring buffer。
4. **按更新频率分层落地**：View/Light CBV per-frame 写一次、Material BindGroup per-material 复用、Object CBV per-draw 动态偏移。SortKey 中 material_id 分箱已有，直接复用。
5. **⭐ 消除 6c SPIRV-Cross 命名债务**：当前 6c 降级 GLSL 的 uniform 名为 SPIRV-Cross 展平后的数组索引形式（`type_PerFrame[N]`），combined sampler 名为自动生成（`_221`/`_223`/`_225`），材质参数表硬编码匹配极其脆弱。本阶段必须通过以下机制彻底解决：
   - **反射元数据驱动**：shader_compiler 输出每个 entry point 的 cbuffer 成员布局 JSON（name → offset/size/type），运行时 `BindGroupLayout` 从反射数据构建，不再依赖字符串匹配。
   - **UBO 绑定替代 glUniform***：cbuffer 保持为真正的 UBO（而非 SPIRV-Cross 展平的 plain uniform），`glBindBufferBase` 按 binding 点绑定，uniform 名称问题自然消失。
   - **Sampler 绑定规范化**：通过 `BindGroup` 声明式绑定纹理+采样器，不再依赖 SPIRV-Cross 自动生成的 combined sampler 名。

**验证**: GL 后端 Sponza 画面无回归；draw call 耗时下降（ImGui 面板 FPS 不降或提升）；D3D12 后端使用同一绑定路径；材质参数表不再包含任何 SPIRV-Cross 生成的内部名称。新增 BindGroup 创建/绑定单元测试。

**对应章节**: 5.13（材质参数绑定与 GPU 上传）、5.14（PreparedMaterial → bind_group + pipeline_key）

> **落地记录（2026-08-08）**：五项子任务全部完成。(1) `ConstantBufferRing`（`render/binding/constant_buffer_ring.h/.cpp`）：GL 持久映射 `GL_UNIFORM_BUFFER`（`glBufferStorage` + persistent/coherent map，缺失时降级 `glBufferData`+`glMapBufferRange`）、D3D12 upload-heap CBV（`createBuffer` 对 cpuAccessible 缓冲持久 Map），256B 对齐按块分配、线性游标满则回卷（8MB 容量 >> 2 帧 in-flight × ~300KB/帧用量）。(2) `BindGroupLayout`/`BindGroup`（`render/binding/bind_group.h/.cpp`）：声明式 binding 描述（cbuffer b 寄存器 + 纹理 t 寄存器 + stage 可见性）；BindGroup 自持 layout 副本（修复 move 后 m_layout 悬垂导致命令洪泛 + 段错误的缺陷），`bind()` 逐 entry 发 `bindConstantBuffer`/`bindTexture`。(3) `Material::bind` 重写：反射驱动的 CPU 常量 blob（cbuffer 成员 name→offset 来自 shader_compiler 输出 `_reflection.json`，`ShaderReflection` 解析）→ 每 cbuffer 一次 memcpy 进 ring → BindGroup 绑定；`setUniform*` 从材质路径彻底移除（GL 翻译器保留命令序列化兼容，D3D12 翻译器删除展平名解析与 staging，`bindConstantBuffer` 按 PSO 反射的 vs/ps cbuffer 解析 binding → root CBV slot 延迟到 draw 提交）。(4) 按更新频率分层：绑定点语义落地为 b0=view/light、b1=material、b2=object（BindGroupLayout 结构），ring 每 draw 全量分配的 per-frame/per-material 复用优化记 TODO。(5) **SPIRV-Cross 命名债务消除**：`flatten_buffer_block` 移除（cbuffer 保持真 UBO `layout(binding=N, std140)`）、420pack 扩展启用（UBO/sampler 带显式 binding）、combined sampler 继承 image 的 Binding 装饰（t-register 即纹理单元）、`_reflection.json` 每 entry point 输出（cbuffer 成员 name/type/offset/size + 纹理 binding）。**配套 shader 变更**：cbuffer 成员改为 vec4/mat4-only（HLSL 打包与 std140 偏移完全一致，单 CPU blob 双后端共用——float3+float、mat3 的 HLSL/std140 会分叉）；同一材质内 cbuffer binding 跨 stage 唯一（GL `GL_UNIFORM_BUFFER` 命名空间共享，sky PS `PerFramePS` 与 simple_cube PS `PerMaterial` 改 b1）；uNormalMatrix 改 float4x4 承载 mat3（shader 内 `(float3x3)` 转型）；GLSL 矩阵输出 `layout(row_major)` + SPIRV-Cross 乘序转置补偿，CPU 照写 column-major `Mat4::m`。**验收**：Debug 构建通过，247 测试全绿（原 240 + 4 BindGroup + 3 ShaderReflection）；GL 与 D3D12 双后端实跑 Sponza 零渲染期 ERROR、剔除统计一致（285 visible / 120 culled）、28 材质全量就绪后截图与 6d 画面一致；材质参数表仅剩真实 HLSL 成员名（grep 验证无 `type_*[N]`/`_2xx`）。**已知限制（记 TODO）**：ring 每 draw 全量分配未做 per-frame/per-material 复用；GL 退出时 `window->destroy()` 先于设备析构的 pending-delete flush，glad 报 1282（既有问题，进程退出无害）。

---

### 6f. Vulkan 接口预留与抽象验证 ⭐

**目标**: 不要求 Vulkan 后端可运行，但要求 6a–6e 的抽象层经过"纸面 Vulkan 适配性审查"，确保不需要再次大规模重构即可接入 Vulkan。

**主要工作**:

1. **Vulkan 适配性清单**：逐项检查 6a 定义的接口是否覆盖 Vulkan 关键概念——VkInstance/VkDevice 双层、VkRenderPass/VkFramebuffer、VkDescriptorSetLayout/VkDescriptorPool、VkPipelineLayout、VkCommandPool/VkCommandBuffer 分配策略、subpass dependencies、pipeline cache。记录缺口到 TODO.md。
2. **`VulkanRHIDevice` 骨架**：仅头文件 + 空实现（全部方法 `assert(false)` 或返回 nullptr），确保编译通过。验证工厂函数、后端枚举、接口继承链路完整。
3. **SPIR-V 加载路径验证**：6c 产出的 SPIR-V 能被骨架 `createShader()` 接受（即使不执行）。
4. **窗口集成预留**：`glfw_window.cpp` 中已有的 Vulkan surface stub（`getNativeDisplay()`）补完签名，但不实现创建逻辑。

**验证**: 骨架编译通过；适配性清单文档化；无阻塞性接口缺陷。

**对应章节**: 5.1（RHI 抽象层——Vulkan 预留）

---

### 6g. PSO 异步编译 + 磁盘缓存 ⭐

**目标**: 解决 shader/PSO 同步编译导致的帧卡顿（尤其 D3D12 首次编译耗时显著），并为 D3D12 Pipeline Library / Vulkan PipelineCache 的磁盘序列化铺路。

**依赖**: 6d（D3D12 后端存在后才有异步编译的实际收益）。

**主要工作**:

1. **`AsyncPSOJob`**：desc + future + placeholder state + 编译状态机（Invalid→Pending→Valid）。后台线程池（idle priority）执行编译。
2. **Placeholder fallback**：编译未完成时使用最近匹配 PSO 或 error-pink 材质，避免白屏/卡帧。
3. **`PSOCompileSystem`**：ECS System 驱动，每帧检查 pending job 完成状态，替换 placeholder。
4. **磁盘缓存**：D3D12 用 `ID3D12PipelineLibrary1` 序列化；VK 用 `vkGetPipelineCacheData`；GL 用 program binary（`glGetProgramBinary`）。版本校验（shader hash + driver version + engine version）。
5. **`PSOManager` ECS 化**：从 `GLRHIDevice` 成员迁出为 `PSOManagerResource`（ECS Resource），解耦后端。

**验证**: 冷启动无可见卡顿；PSO 缓存命中率统计正常；磁盘缓存在二次启动时跳过编译。

**对应章节**: 5.3b（PSO 缓存与异步编译）

---

### 阶段 6 子任务依赖关系

```
6a (RHI 接口审计) ──┬──→ 6b (延迟命令缓冲) ──┐
                    │                          ├──→ 6d (D3D12 后端) ──→ 6g (PSO 异步)
                    ├──→ 6c (着色器编译)   ──┘
                    │
                    └──→ 6e (UBO/CBV 绑定) ────→ (与 6d 合流)
                    
6f (Vulkan 预留) ← 6a 完成后随时可做，不阻塞其他任务
```

**推荐执行顺序**: 6a → 6b + 6c（可并行）→ 6e → 6d → 6f → 6g

---

## 阶段 7+ —— 高级渲染特性（阶段 6 完成后按需推进）

阶段 6 建立多后端架构后，以下特性均基于新架构实现，每项独立完成、可随时停下：

1. **5.7 RenderGraph 最小实现**（<500 行编译器：拓扑排序 + 死 Pass 剔除 + Barrier 插入）——为阴影 pass、后处理铺路。RenderGraph 的 transient 资源别名在多后端下尤其重要（D3D12 heap aliasing / VK memory aliasing）。
2. **5.11/5.12 材质 TAI 三层 + 着色器变体系统**——`ShaderTemplate` → `MaterialAsset` → `MaterialInstance`，keyword-driven permutation，替换 Phase 1 简化 Material。与 6c 着色器工具链深度集成。
3. **5.5 BVH 空间加速**——Sponza 数百实体 CPU 暴力剔除尚可，场景变大后必做。Static BVH + Dynamic List 双轨制。
4. **5.15/5.16/5.17 2D/字体/UI**——SpriteBatch + MSDF 字体 + UI 画布，自研 UI 框架前置。
5. **5.18/5.19 后处理栈**——ToneMapping / Bloom / TAA / SSAO，依赖 RenderGraph + HDR SceneColor。
6. **5.10 资源热重载**——FileWatcher + 帧边界替换，开发效率关键。
7. **5.20 基础粒子系统**——SoA 存储 + GPU 升级路径预留。
8. **5.2 多线程命令录制 Phase 3**——并行 command list 录制 + range-slice + 有序 submit（D3D12/VK 专属优化）。
9. **Bindless / GPU-Driven Rendering**——D3D12 Shader Model 6.0+ / VK descriptor indexing，终极渲染性能路径。

---

## 跨阶段注意事项

- **每阶段验收都跑 Debug 构建**（`python scripts/build/build.py --debug --build`），并保证既有单元测试不红。
- **文档同步**：每阶段结束后更新 `docs/RENDER_LAYER_PROGRESS.md` 对应章节的"代码现状"与完成度条，以及相关模块 `AGENTS.md`。
- **遇阻即停**：glTF 解析/cook（accessor 布局、交错顶点流、V 翻转）、metallicRoughness 打包纹理解包两处是已知风险点，卡住时按 AGENTS.md 规则停下汇报，不自行换方案绕过。原三大高风险点（Unity YAML 解析、FBX 导入、HDRP 参数映射）随资产更换已消解。
- **多后端新增风险点**：(1) DXC/SPIRV-Cross 的 Conan 集成可能遇到版本兼容问题；(2) D3D12 调试需 PIX/Nsight，确认本机工具可用；(3) HLSL←→GLSL 语义差异（坐标系、纹理采样、整数运算）可能导致画面不一致，需逐 shader 校对；(4) D3D12 资源屏障遗漏会导致难以排查的画面错误，初期保守插入屏障、后期由 RenderGraph 优化。卡住时同样停下汇报。
- **不做的事**：阶段 6 不追求性能优化（Bindless/GPU-Driven/HZB）、不追求渲染品质（阴影/TAA/Bloom）——这些属于阶段 7+。阶段 6 的目标是**架构正确性**，不是性能或画质。
- 每阶段完成后按 AGENTS.md「事后审视」规则，把偏差与新技术债务记入 `TODO.md`。
