# 阶段 5「让它像样」详细执行计划

> **生成日期**: 2026-08-06
> **父文档**: [RENDER_SPONZA_ROADMAP.md](RENDER_SPONZA_ROADMAP.md) 阶段 5
> **拆分原因**: 阶段 5 四项工作（光照 / 法线贴图 / 天空 / 调试统计面板）中，光照是从零起（代码库无任何 Light 概念）且 shader 需整体重写为 lit PBR，属本阶段最大风险点；法线贴图的视觉验收**依赖光照先成立**（无光时法线贴图对错不可见），天然排在光照之后；天空与统计面板是独立小件。沿用阶段 2/3/4 的拆法：每步独立验收、可独立提交。
> **目标资产**: `_content/sponza/`（已全贴图上屏；normal/MR 贴图已 loadAsync 落位未采样，阶段 4c D4）
> **可见终点**: 有方向光（阳光）照射的 NewSponza，明暗正确、法线贴图生效；天空渐变；ImGui 面板显示 FPS / draw calls / 剔除统计 / GPU 显存（5c 验收）

---

## 现状关键事实（2026-08-06 调研）

- **Shader 是硬编码字符串，且是 unlit**：唯一 shader pair 在 `render_system/private/prepare/PrepareAssetsSystem.cpp:22-45`（`#version 330 core` 内联 GLSL，全仓无 .glsl 文件）。vs 只用 `aPos`/`aUV`；normal/tangent 属性已绑定（loc 1/3）但 shader 不读。fs 只采样 `uBaseColorTex`。材质参数表仅 4 项（`uMVP`/`uColor`/`uAlphaCutoff`/`uBaseColorTex`）。光照 = fs 整体重写，不是增量补丁。
- **`uModel` 已随 shade_mode 退役拆除**（4b）：Execute 目前只传 `uMVP = proj*view*world`。光照需要世界空间位置与法线，必须恢复 per-draw 世界矩阵下发（并补法线矩阵，见 D3）。
- **`ExtractedView` 无相机世界位置**（`render_system/public/components/RenderCamera.h:20-28`）；PBR 需要 viewDir。`ExtractCameraSystem.cpp:29` 处 `transform->matrix` 现成，补一个字段即可。
- **光源零存在**：`Light|DirectionalLight|PointLight` 全词搜索在 `_engine/source` 与 `_game/source` 零命中。组件、Extract、uniform 全部从零起。
- **uniform 全走 `glUniform*` 立即模式**（`Material::bind()` 遍历上传，无 UBO/BindGroup）。1 盏方向光 + 1 个相机位置用现有机制完全可接受；UBO/按频率分层是 5.13 既有债务，阶段 6+ 处理，本阶段不碰。
- **法线贴图数据链已完整**：`MeshVertex` 含 `tangent(Vec3)+tangentW(f32)`（`asset/public/type/mesh_asset.h:18-24`），cooker 已解码 glTF TANGENT（含 handedness）并写入 .emesh，GPU 侧 loc 3 = vec4(tangent.xyz, tangentW) 已绑定。**只欠 shader 消费**。UV 已在 cooker 侧 V 翻转（4b D2），normal/MR 与 baseColor 共用同一 UV 约定。
- **normal/MR 贴图 Handle 已落位**：`MaterialAsset::normal_texture`/`mr_texture` 经 `MaterialTextureBackfillSystem` 回填（4c），`PrepareAssetsSystem::prepareMaterial` 目前只 prepare baseColor（`PrepareAssetsSystem.cpp:254-333`），扩展点现成。`metallic_factor`/`roughness_factor` 已在 `MaterialAsset` 中但未传 shader。
- **Clear color 在游戏主循环**：`build/generated/main.cpp:212-214`（生成文件，源在 `launch/templates/`）；ImGui Debug 面板已有 Clear Color 编辑器（`imgui/private/imgui_panels.cpp:57`）。无 skybox/sky 渲染，Execute 目前不拥有 pass 边界——天空需要在 clear 之后、opaque 之前插一个全屏 pass。
- **统计接口现成未接**：`IRHIDevice::queryMemoryInfo()`/`getTrackedMemoryUsage()`（`render/public/rhi/rhi_device.h:65-70`）与 `PSOManager` 统计均存在；`buildRenderStatsPanel` 目前只显示 draw calls/visible/culled/total 四行（`imgui_panels.cpp:128-141`），调用点在生成的 `main.cpp:196-197`。
- **知识库**：vault 已有《让像素响应光》（Blinn-Phong）、《数据通道与更新频率》、《法线变换的几何直觉》（法线矩阵 = 逆转置）三篇光照基础笔记，直接作为 D3/D4 依据；**GGX 实现细节与 MR 打包约定 vault 无笔记**（已记 `Note_TODO.md`），实现时参照 learnopengl.com PBR 章节等外部资料。
- **glTF lights 为空**：主文件声明 `KHR_lights_punctual` 但 lights 数组为空，光照参数自配（参照 `_content/sponza/Render_Main_*.png` 官方渲染图的日光角度）。

---

## 决策（已定）

- **D1 光照模型：单方向光 + 常量环境项，直接上 GGX Cook-Torrance**。roadmap 明确「材质转换器已导出 metallic/roughness，直接上 GGX 更值」。fs 重写为 lit PBR：albedo（baseColor 贴图×factor）、metallic/roughness factor（5a 先用 factor，贴图相乘属 5b）、Lambert diffuse + GGX specular（D=GGX/TR、F=Schlick、G=Smith）。环境项 = 常量 ambient×albedo，**不做 IBL/半球环境光**（阶段 6+）。shader 仍是 PrepareAssetsSystem.cpp 内联字符串——不引入 shader 文件机制（属变体系统，阶段 6+）。
- **D2 光源 ECS 化**：主 World 新增 `DirectionalLight` 组件（direction/color/intensity）+ `ExtractLightSystem` 拷贝进 Render World（取第一个方向光，与 ExtractCameraSystem 同模式）；游戏侧在 `GamePlugin::setup()` spawn 一盏，初始参数参照官方渲染图日光角度。**光照方向/颜色/强度接入 ImGui Debug 面板可调**（复用现有面板机制）——日光角度靠目视对齐官方图，运行时可调比改常量重编译成本低一个量级。uniform 仍走现有 glUniform* 立即模式。
- **D3 世界矩阵与法线矩阵恢复下发**：Execute 每 draw 传 `uModel`（世界矩阵，uMVP 合成处现成）+ `uNormalMatrix`（mat3，CPU 侧算 world 的逆转置——依据 vault《法线变换的几何直觉》，非均匀缩放必须逆转置；405 draws 每帧一次 3×3 求逆代价可忽略）。vs 输出世界空间 position/normal 供 fs 光照。
- **D4 相机位置入 `ExtractedView`**：新增 `view_pos` 字段，`ExtractCameraSystem` 从相机 transform 提取；Execute 下发 `uViewPos`。
- **D5 法线贴图走顶点 TANGENT（5b）**：TBN = T/B/N，B = cross(N,T)×tangentW；normal map 采样值为 tangent-space，直接 `N = TBN * (tex*2-1)`。**不做法线矩阵参与 TBN 的完整处理**——T/N 同受 world 变换，5b 实现时用 uNormalMatrix 变 N、mat3(uModel) 变 T 后正交化即可，细节实现时按 vault 法线矩阵笔记对齐。MR 解包：G=roughness、B=metallic，与 factor 相乘；无贴图材质退化为 factor（hasTexture 分支沿用现有 shader 手写分支模式，不做变体系统）。
- **D6 天空 = 全屏渐变 pass（5c）**：clear 之后、opaque 之前画一个全屏三角形（depth test always + depth write off），按视线方向 y 分量在地平线色/天顶色之间插值；两色常量 + ImGui 可调。**不做天空盒**（资产包不含，roadmap 明确另行准备）。实现位置：`RenderExecuteSystem` 内 view 级前置步骤，或 `RenderFrameRunner` 编排——实现时按 Execute 现有结构取侵入更小者。
- **D7 统计面板扩展（5c）**：`buildRenderStatsPanel` 增加 FPS、total/visible/culled（已有）、PSO cache 统计、GPU 显存（`queryMemoryInfo`/`getTrackedMemoryUsage`）；数据链：`FrameStats` 扩展或由 main 直接查询 device，改动落在 `imgui_panels.cpp` + `launch/templates/main.cpp.in`。
- **D8 Gamma 最小处理**：fs 内近似 gamma——albedo 采样后 `pow(2.2)` 转线性，最终输出 `pow(1/2.2)`。不改纹理内部格式/不启用 SRGB framebuffer（属正确色彩管理，记 TODO.md）。理由：完全不处理时 PBR 观感明显偏亮，两行 pow 换"像样"达标。

---

## 5a —— 方向光 + lit PBR shader（核心视觉验收）

**目标**：窗口中 Sponza 被方向光照射，明暗关系正确（向阳面亮、背阴面暗但有环境光兜底）、高光随视角移动；光照参数经 ImGui 调节后与官方渲染图观感一致。

**范围**：

1. `DirectionalLight` 组件（主 World，direction/color/intensity）+ 组件注册 + `ExtractLightSystem` → Render World `ExtractedLight`（取第一个，同相机模式）；游戏侧 spawn 一盏。
2. `ExtractedView` 补 `view_pos`（D4）；`ExtractCameraSystem` 提取。
3. Execute：`uModel`/`uNormalMatrix`/`uViewPos` + 光照 uniform（`uLightDir`/`uLightColor`/`uLightIntensity`/`uAmbient`）下发（D2/D3/D4）。
4. fs 重写为 lit PBR（D1/D8）：vs 输出 worldPos/worldNormal/uv；fs = Lambert + GGX + 常量环境项 + 近似 gamma；保留现有 `uAlphaCutoff` discard 分支与 doubleSided（`gl_FrontFacing` 翻法线）。
5. ImGui Debug 面板加光照方向/颜色/强度/环境项调节（D2）。
6. 测试：`ExtractLightSystem` 配单测（Extract 拷贝语义，同 ExtractCameraSystem 既有测试模式）；法线矩阵若抽 helper（math 层 inverse-transpose 3×3）配单测；shader/装配部分沿用 3c/4b 先例不配单测。

**验收**：Debug 构建通过；既有 207 测试不红 + 新增测试绿；lint 干净。实跑：零 ERROR/WARN、零 pending/fallback 残留、`Frame stats` 无回归（draw calls/剔除/帧率同 4c 基线）；**目视验收**（沿用 3c/4b 用户目视方式）：向阳/背阴关系正确、高光随相机移动、调节 ImGui 光照参数后与 `Render_Main_*.png` 观感一致。GGX 公式写错是唯一无法自动验证的风险点——若画面异常（全黑/全白/高光爆炸）且 30 分钟内定位不了，按「遇阻即停」汇报。

> **落地情况（2026-08-06）**：范围 1–6 全部落地。`DirectionalLight`（direction/color/intensity/ambient，`render_system/public/components/DirectionalLight.h`）+ 组件注册 + `ExtractLightSystem`（取第一个、direction 归一化、零长度回退朝下）→ Render World `ExtractedLight`（`RenderLight.h`）；`ExtractedView` 补 `view_pos`（取相机 transform 平移列）；Execute 每 draw 下发 `uMVP`/`uModel`/`uNormalMatrix`/`uViewPos`/`uLightDir`/`uLightColor`/`uLightIntensity`/`uAmbient`；fs 重写为 lit PBR（Lambert + GGX Cook-Torrance + 常量环境项 + 近似 gamma，保留 mask discard 与 doubleSided 翻法线），vs 输出 worldPos/worldNormal/uv；`metallic_factor`/`roughness_factor` 顺手接入（`uMetallic`/`uRoughness`）；游戏侧 spawn 暖色日光（direction≈(0.45,-0.85,-0.25)、(1.0,0.956,0.839)、intensity 3.0、ambient 0.03），ImGui Debug 面板方向/颜色/强度/环境项可调（`DirectionalLightParams` POD 镜像，不改 ImGuiLib 依赖）。**偏差**：Material 参数机制不支持 mat3（计划预判的遇阻点）——按「基础设计优先」正路扩展而非绕路：core math 新增 `Mat3`（inverse/transpose/`normalMatrix`），`MaterialParamType`/`Material::setMat3`/`IRHICommandList::setUniformMat3`/`GLCommandList`（glUniformMatrix3fv）全链补齐。**验收**：Debug 构建零编译/链接错误；EntelechyTests 220 全绿（207 基线 + 8 Mat3 + 5 ExtractLightSystem）；lint 全仓 239 文件零违规。实跑 35s（`logs/game_run_5a.log`，timeout 终止）：405 实体、零 ERROR/WARN、pending/fallback 日志仅启动期（最后一条在 986 行的第 457 行）、`draw_calls == visible`、fps 56–61 与 4c 基线相当；落地材质/贴图数与 4c 同模式（异步加载时序差异，4c 基线同样未在窗口期内全数 prepare，非 5a 回归）。新债务已记 TODO.md（gamma 近似非 SRGB、ambient 常量化无 IBL、光照 uniform 逐 draw + 法线矩阵每 draw 重算）。**目视验收待用户确认**。
> **截图自验与修正（2026-08-06，二）**：用户指出天花板纯色异常，要求 agent 自行截图验证。截图链路：游戏后台跑 + PowerShell `CopyFromScreen` 全屏截图（脚本 `logs/capture_screen.ps1`；注意 `build.py` 会清 `.cache/`，产物勿放该目录），临时改初始相机位姿拍天花板视角后还原。**根因**：cooker 按 glTF spec 默认值导出 `metallic_factor=1`（全部 25 个贴图材质）——factor 是 MR 贴图的乘数而非独立常量，5a 未采样 MR 贴图（5b 才接）时当常量用，全场 kD=(1-F)(1-metallic)=0 漫反射归零，只剩高光+环境项，背光天花板因此纯色。**修复**：`prepareMaterial` 对有 MR 贴图的材质用占位 metallic 0.0/roughness 0.9（factor-only 材质保持真 factor），5b 采样后恢复 factor×texture 语义；ambient 默认值 0.03→0.15（0.03 下背光面≈全黑）。**天花板残余平坦属预期**：ceiling_plaster_01/02 BaseColor 贴图本身极低对比度 + 拱顶背光面只有常量环境项（无 GI/IBL/AO），官方图的拱顶明暗来自 GI 反弹，阶段 5 范围内无解（已记 TODO ambient 条目）。截图存档 `logs/shot_ceiling2.png`（修复后天花板视角）、`logs/shot_default_fixed.png`（修复后默认视角：石材漫反射明暗与贴图细节恢复）。修复后复验：EntelechyTests 220 全绿、lint 239 文件零违规、实跑帧统计无回归（draw_calls=285=visible、culled=120/405）。

---

## 5b —— 法线贴图 + MR 贴图采样

**目标**：砖墙、拱门、地砖呈现法线贴图细节（凹凸随光照角度变化）；metallic/roughness 贴图生效（金属件与石材高光响应不同）。

**范围**：

1. Prepare：`prepareMaterial` 扩展——`normal_texture`/`mr_texture` Handle 有效则 prepare 成 RHI 纹理随材质下发（复用 baseColor 的「未就绪→fallback→热替换」既有机制与 pending 守卫）。
2. shader：vs 输出 TBN（D5）；fs 采样 normal map 扰动 N、采样 MR 贴图与 factor 相乘；无贴图材质走 factor-only 分支（手写分支，沿用现有模式）。
3. 验收重点：handedness（tangentW）与 V 翻转的交互——砖缝凹凸方向（光照下凹凸是否反转为"浮雕变凹雕"）是本子步唯一实需目视确认的风险点。

**验收**：Debug 构建通过；既有测试不红；lint 干净。实跑：73 贴图全部 prepare 落地（4c 只加载未 prepare，本步开始 normal/MR 真正上传）、零 ERROR/WARN、帧率无回归（73 张 PNG 中部分 4K，显存增长属预期，记录数字为 5c 显存面板校对依据）；目视验收法线贴图细节与 MR 高光差异。凹凸方向反转若出现且定位超 30 分钟，停下汇报（不两侧乱翻 handedness/UV）。

> **落地情况（2026-08-06）**：范围 1–2 全部落地。`prepareMaterial` 扩展——normal/MR 贴图 Handle 有效则 `prepareTexture` 成 RHI 纹理随材质下发，复用 baseColor 的「path 无 Handle→pending」「贴图未上传→pending 粉」「异步落地后热替换」全套机制；材质参数表 13→17 项（新增 `uNormalTex`/`uMRTex`/`uHasNormalTex`/`uHasMRTex`，fallback 材质绑定白图 + 双 flag=0）。shader 按 D5：vs 消费 loc 3 `vec4(tangent.xyz, tangentW)`，N 走 `uNormalMatrix`、T 走 `mat3(uModel)` 后 Gram-Schmidt 正交化输出；fs 以 `B = cross(N,T) * tangentW` 重建 TBN，法线贴图（tangent-space，tex*2-1）扰动 N，MR 贴图 G=roughness/B=metallic 与 factor 相乘（clamp metallic [0,1]、roughness [0.05,1]）；无贴图材质经 `uHasNormalTex`/`uHasMRTex` 手写分支走 factor-only。**5a 的 metallic 0.0/roughness 0.9 占位已移除**，恢复 factor×texture 语义（TODO.md 对应条目已勾选）。`material_asset.h` 注释同步（normal/MR 自 5b 起被采样）。**验收**：Debug 构建零编译/链接错误；EntelechyTests 220 全绿；lint 全仓 239 文件零违规（`--staged` 无暂存文件，改跑全仓）。实跑（`logs/game_run_5b.log`）：73 张贴图全部 prepare 上传（含 4K，RGBA8 无 mipmap 合计约 **4672 MiB** GPU 纹理显存——5c 显存面板校对基准）、28 材质全部 ready（normal=1 mr=1）、零 ERROR/WARN、pending/fallback 日志仅启动期（最后 pending 在第 457 行，材质 31/32 随后于 1082/1089 行 ready）、稳定 60 fps、`draw_calls == visible`（285/285、culled 120/405，与 5a 基线一致，无回归）。**截图验收**（`logs/shot_5b_default.png`，默认视角）：砖拱砌缝凹陷、石块凸起受光方向正确（光从左上来，凸起顶/左缘亮、凹槽暗，无「浮雕变凹雕」反转）；石柱/墙面法线颗粒细节清晰；木门（带金属件）呈柔和光泽、与石材的宽 rough 响应差异可见。已知偏差：本次游戏窗口启动后被最小化（反复 `WindowResize 0x0`，期间 `visible=0/culled=405`），经 PowerShell `ShowWindow` 恢复后正常——属环境/窗口管理问题，非 5b 改动引入；截图机制仍是外部 PowerShell 脚本（TODO.md 已有引擎内截图条目）。**目视验收待用户确认。**
>
> **mipmap 修复（2026-08-06，二）**：上述截图复查发现全场高频颗粒噪点（砖墙/石柱缩远处尤甚）。根因：`prepareTexture` 上传时 `mipLevels=1`，GL 侧退化为 `GL_LINEAR` 无 mip 过滤，4K 法线贴图缩小采样严重 aliasing。修复：`prepareTexture` 按 `max(w,h)` 计算完整 mip 链级数（`glGenerateMipmap` + `GL_LINEAR_MIPMAP_LINEAR` 路径 RHI 已有，`textureMemorySizeBytes` 本就算 mip 链）。复验：重建 + EntelechyTests 220 全绿 + lint 零违规；实跑零 ERROR/WARN、60 fps 无回归；截图（`logs/shot_5b_mip.png`）噪点消除、砖缝细节保留。注意：mip 链使纹理显存约 +33%（5c 显存面板数字应高于 4672 MiB 基准，约 6.2 GiB 量级）；此前无 mipmap 属隐性画质缺陷，baseColor 阶段未暴露是因 albedo 多为低频。

---

## 5c —— 天空 + 调试统计面板

**目标**：天空不再是纯色 clear，而是竖直渐变；ImGui Render Stats 面板显示 FPS、draw calls、visible/culled/total、PSO cache 统计、GPU 显存。

**范围**：

1. 天空渐变 pass（D6）：全屏三角形 + 方向插值 shader，clear 后 opaque 前；颜色 ImGui 可调。
2. 统计面板扩展（D7）：FPS、PSO cache（`PSOManager` 统计）、GPU 显存（`queryMemoryInfo`/`getTrackedMemoryUsage`，5b 记录的实数可校对）；`FrameStats` 按需扩展；改动 `imgui_panels.cpp` + `launch/templates/main.cpp.in`。
3. 文档同步：`docs/RENDER_LAYER_PROGRESS.md` 5.13/5.14 章节「代码现状」与完成度（光照/贴图采样/面板落地）；roadmap 阶段 5 标记完成；偏差与债务记 `TODO.md`（预期至少含：gamma 近似非 SRGB 正确管线、ambient 常量化无 IBL、光照 uniform 仍 glUniform 立即模式、天空盒未做）。

**验收**：Debug 构建通过；全量测试绿；lint 干净。实跑：天空渐变无深度冲突（场景几何不被天空覆盖、远处无几何处见渐变）；面板各数字合理（显存数字与 5b 记录吻合、draw calls == visible）；帧率无回归。

---

## 跨子步注意事项

- 每子步结束跑 `python scripts/build/build.py --debug` + 全量测试 + `python scripts/tools/lint.py --staged`。
- 每子步完成后更新 `docs/RENDER_LAYER_PROGRESS.md` 对应章节「代码现状」；偏差与新技术债务记入 `TODO.md`。
- **遇阻即停**：GGX 公式正确性（5a）与法线贴图凹凸方向（5b）两处是已知风险点，卡住停下汇报，不绕路、不两侧乱改。
- 视觉验收仍依赖用户目视（引擎无截图机制，TODO.md 已有条目）；5a/5b 每步都需用户确认观感后进入下一步。
- 不做：阴影、多光源、IBL、UBO/BindGroup 分层（5.13 债务，阶段 6+）、shader 变体系统、blend 正确混合（4b 已记 TODO）、色彩管理（SRGB，记 TODO）。
