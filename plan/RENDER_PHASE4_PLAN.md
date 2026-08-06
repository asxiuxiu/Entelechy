# 阶段 4「还原材质与场景」详细执行计划

> **生成日期**: 2026-08-06
> **父文档**: [RENDER_SPONZA_ROADMAP.md](RENDER_SPONZA_ROADMAP.md) 阶段 4
> **拆分原因**: 阶段 4 四项工作（材质 JSON 格式 + loader / 贴图上屏 / 贴图正确性风险 / scene_loader 迁引擎）串行依赖，且 V 翻转、alpha/doubleSided 两个视觉风险需要独立的实跑验收环节，不能与格式设计、搬迁搅在一起调试；沿用阶段 2/3 的拆法，每步独立验收、可独立提交。
> **目标资产**: `_content/sponza/`（28 个 glTF pbrMetallicRoughness 材质，137 张 PNG：BaseColor/Normal/Roughness/Metalness 及预打包 RoughnessMetalness）
> **可见终点**: 全贴图 NewSponza——砖墙、拱门、地砖、旗帜各就各位（4b 验收）

---

## 现状关键事实（2026-08-06 调研）

- **Prepare 贴图链路已全部就绪**：`MaterialAsset` 已有 `Handle<TextureAsset> base_color_texture`（`asset/public/type/material_asset.h:27`）；`PrepareAssetsSystem::prepareMaterial` 已实现「贴图未就绪 → 白纹理/粉色 fallback → 就绪后重建材质」热替换（`render_system/private/prepare/PrepareAssetsSystem.cpp:280-294`）；`prepareTexture` 上传路径 2c 已验证。阶段 4 在 Prepare 侧只需扩展字段与管线变体，不是从零做。
- **`MaterialAssetLoader` 不存在**。模板现成：`MeshAssetLoader`（3a）与 `TextureAssetLoader`；注册模式为 `RenderAssets` 持 loader 实例、按路径 `loadAsync`（AssetServer 无扩展名注册表）。JSON 解析用 core `JsonCursor`（`core/json/json_cursor.h`，3c 起共享）。
- **cooker 只导出了材质名占位**：`mesh_cooker/private/main.cpp:308` 取 `prim->material->name`，`scene.json` 的 `material` 字段是字符串占位（`main.cpp:336`）；cgltf 材质结构体（pbrMetallicRoughness / alphaMode / doubleSided）尚未触碰。cooker 侧手写 JSON emit 先例现成。
- **V 翻转未处理**：`TextureAssetLoader`（`asset/private/loader/texture_asset_loader.cpp`）未调 `stbi_set_flip_vertically`，stb 顶行优先解码 + OpenGL 底左原点 ⇒ glTF UV（原点左上）直采必然上下颠倒。白模阶段不采样贴图，此问题本阶段首次暴露。
- **scene_loader 仍属游戏侧但依赖已全部引擎化**：`_game/source/runtime/private/scene_loader.cpp`（149 行）只用 core `JsonCursor`、引擎组件（`MeshAssetRef`/`MaterialAssetRef`/`WorldAABB`）与 `AABB::transformed`，搬迁无新依赖，主要是 CMake/命名空间调整 + `GamePlugin` 改为只传场景路径。
- **alpha/doubleSided 是 roadmap 未点名的预期风险**：NewSponza 的帘幕/格栅类材质几乎必然带 `alphaMode: MASK` 与 `doubleSided: true`，当前管线 `CullMode::Back` + 无 alpha test 会出现破洞/半消失。4a cooker 先统计导出，让 4b 范围有实据。
- **法线/MR 贴图本阶段无法视觉验收**：光照在阶段 5，normal map / metallicRoughness 解包（G=roughness、B=metallic）采了也看不出对错。按 roadmap「不追求渲染品质」原则，本阶段只做**解析 + 加载落位**，shader 采样留阶段 5（对 roadmap 阶段 4 第 3 条的有意收窄，见 D4）。

---

## 决策（已定）

- **D1 材质清单格式：每材质一个 `.emat` JSON 文件**。cooker 输出 `materials/<name>.emat`（字段：`base_color_texture`/`normal_texture`/`mr_texture` 的内容相对路径、`base_color_factor`[3]、`metallic_factor`、`roughness_factor`、`alpha_mode`（opaque/mask/blend）、`alpha_cutoff`、`double_sided`）；`scene.json` 实体 `material` 字段由名字占位改为 `.emat` 相对路径。理由：与 mesh/texture 的「一文件一资产 + `IAssetLoader<T>` + `loadAsync`」模型同构，spawner 侧按路径去重（28 个唯一材质，小 map 即可），无需为材质发明清单内嵌新机制。格式归引擎（cooker 是引擎工具），loader 落 `asset/private/loader/material_asset_loader.cpp`。设计前按 AGENTS.md 规则先检索知识库材质相关笔记（TAI 三层不做，但 schema 字段命名避免与未来冲突）。
- **D2 V 翻转修在 cooker（UV 侧）**：cook 时 `v = 1 - v` 写入 `.emesh`，纹理 loader 不动。理由：`.emesh` 是 glTF 派生格式，翻转属于几何侧约定，与法线/切线同源；`stbi_set_flip_vertically` 是全局开关，会污染 demo 棋盘格等既有用途。代价：`.emesh` 格式语义变化需重跑 cook（产物本就不入 git，零成本）。**若 4b 实跑发现贴图仍颠倒，说明判断有误，停下汇报而非两侧乱翻。**
- **D3 alpha/doubleSided 最小处理**：`MaterialAsset` 增加 `alpha_mode`/`alpha_cutoff`/`double_sided` 字段；Prepare 侧按材质选管线变体（`double_sided` → `CullMode::None`；`mask` → fs `discard < cutoff` 的 shader 分支）；`blend` 本阶段按 opaque 处理并记 TODO（正确混合需要排序，属阶段 5+ 或更后）。不做通用 PSO 变体系统——两三个手写分支即可，TAI/变体缓存是阶段 6+。
- **D4 法线/MR 贴图只加载不采样**：4a 导出路径与 factor 字段、4c 经 `loadAsync` 落位到 `Assets<TextureAsset>`；Prepare 不上传不绑定（shader 无消费端），阶段 5 光照落地时再接。roadmap 阶段 4 第 3 条「Prepare 阶段绑定贴图」相应收窄为 baseColor。
- **D5 scene_loader 迁引擎（4c）**：落 `asset/` 或独立 scene 模块（实现时按模块归属判断，倾向 `asset/scene/`——`scene.json`/`.emat` 均为引擎 cooker 自有格式）；游戏侧 `GamePlugin::setup()` 只传场景路径。`RenderAssets` 中 Sponza 相关初始化（`mat_white` 等）随迁移清理，demo 资产（cube/棋盘格）保留原处。

---

## 4a —— 材质导出（cooker）+ 引擎材质 loader ✅ 已完成（2026-08-06）

> **落地情况**：`MaterialAsset` 扩展（`asset/public/type/material_asset.h`）：`metallic_factor`/`roughness_factor`（默认 1.0/1.0）、`normal_texture`/`mr_texture` Handle（本阶段保持无效，回填属 4b）、`AlphaMode`（Opaque/Mask/Blend，默认 Opaque）、`alpha_cutoff`（0.5）、`double_sided`；`base_color` 复用为 baseColorFactor 语义，保持 Vec3、丢弃 A（代码注释说明：常量级 alpha 在光照阶段前无消费端）。三个 `*_texture_path`（core `String`，default-constructible 与 HandleTable 兼容，未用定长缓冲）存贴图内容路径——loader 按 D1 只解析不触发贴图加载，路径/Handle 双存的过渡性已记 TODO.md。cooker（`mesh_cooker/private/main.cpp`）新增 `cookMaterials()`：每材质写 `materials/<name>.emat` 手写 JSON（schema 见函数注释），贴图 URI 百分号解码、相对 `.gltf` 目录解析后转 `_content/` 相对内容路径，材质名清洗为 `[A-Za-z0-9_-]`（Sponza 28 个名字全部干净，清洗未实际触发）；`scene.json` 实体 `material` 写 `.emat` 清单相对路径，无材质 primitive 写空串并告警（Sponza 未触发）；无 pbr 块材质按 glTF spec 默认值补齐。`MaterialAssetLoader`（`asset/public/loader/material_asset_loader.h` + `private/loader/material_asset_loader.cpp`）用 core JsonCursor 按键名分发解析：缺字段走默认值、空对象合法；垃圾/截断/错误类型/未知 alpha_mode/未知键一律拒绝（返回默认材质 + 错误日志）。偏差一处：计划要求缺 baseColor 贴图告警，实现改为只计数不告警（factor-only 材质是合法 glTF，告警会破坏零告警回归基线），统计数字不丢。
> **验收**：Debug 构建通过；EntelechyTests 207 全绿（197 既有 + 10 新增：完整往返/缺字段默认/空对象/alpha 映射/垃圾/截断/错误类型/未知 alpha/未知键/无效 FileData）；lint 干净。cooker 实跑零告警：405 primitives 全 cook（0 跳过）、`scene.json` 405 实体 material 字段全部指向存在的 `.emat`（脚本校验 0 空 0 缺失）、28 个 `.emat` 全部生成且 JSON 可解析、引用贴图路径全部存在于 `_content/`；统计：**28 材质中 mask 0 / blend 1（dirt_decal）/ doubleSided 2（lamp_glass_01、glass）/ 缺 baseColor 贴图 3（lamp_glass_01、light_bulb、glass）**——4b 的 alpha 工作量只有 1 个 blend 材质（按 D3 当 opaque）+ 2 个 doubleSided 管线变体，无 mask 分支实需。抽查 brickwall_01（三贴图全、factor 全 1）、dirt_decal（blend、仅 baseColor）、glass（factor-only、doubleSided、factor 全 0）内容正确。游戏侧未改：`scene_loader` 把 material 当字符串跳过，路径字符串兼容，全量构建通过即无行为破坏。

**目标**：cooker 解析 cgltf 材质 → 输出 28 个 `.emat` + `scene.json` 材质字段改为路径；`MaterialAsset` 扩展字段；新增 `MaterialAssetLoader` 并配单元测试。cooker 打印 alphaMode/doubleSided 统计为 4b 定范围。

**范围**：

1. `MaterialAsset` 扩展：`base_color_factor`（复用现有 `base_color`）、`metallic_factor`/`roughness_factor`、`normal_texture`/`mr_texture` Handle、`alpha_mode`/`alpha_cutoff`/`double_sided`。保持 default-constructible（HandleTable 要求）。
2. cooker（`_engine/tools/mesh_cooker/`）：遍历 `data->materials` 导出 `.emat`（cgltf 的 `pbr_metallic_roughness` 字段 + image URI → 内容相对路径映射，注意 URI 相对 `.gltf` 所在目录解析）；`scene.json` 实体 `material` 写 `.emat` 相对路径；无材质 primitive 写空并告警。统计打印：28 材质中 mask/blend/doubleSided 各多少、缺 baseColor 贴图的多少。
3. `MaterialAssetLoader : IAssetLoader<MaterialAsset>`：JsonCursor 目的解析 `.emat`；纹理字段存**路径**还是直接触发加载取实现最简单者——倾向 loader 只解析路径字符串，由场景 spawn 侧统一 `loadAsync` 后回填 Handle（避免 loader 反向依赖 AssetServer；与 mesh loader 的无依赖形态一致）。
4. 测试：`.emat` 往返（构造 MaterialAsset 字段期望 ↔ JSON 解析）、缺字段默认值、垃圾/截断拒绝；cooker 无新增单测（先例 3b）。

**验收**：Debug 构建通过；既有 186 测试不红 + 新增测试绿；lint 干净。cooker 重跑：405 primitive 零告警回归、28 个 `.emat` 全部生成且 JSON 可解析、`scene.json` 405 实体材质路径均指向存在文件；alpha/doubleSided 统计数字落入计划注释（界定 4b 工作量）。

---

## 4b —— baseColor 贴图上屏（核心视觉验收）✅ 已完成（2026-08-06）

> **落地情况**：场景侧材质装配（游戏侧，结构未动，迁移属 4c）：`scene_loader` 解析实体 `material` 的 `.emat` 清单相对路径 → 按路径经 `HashMap<String, Handle>` 去重 `loadAsync`（`RenderAssets` 新增 `MaterialAssetLoader material_loader` 成员，同 mesh/texture loader 模式）→ 实体挂各自 `MaterialAssetRef` 替换共享 `mat_white`；唯一材质 Handle 记入 `RenderAssets::scene_materials`（`Assets<T>` 不支持遍历/可变迭代，另存清单是最简可行方案）。贴图 Handle 回填：新增 `MaterialTextureBackfillSystem`（scene_loader.h/.cpp，GamePlugin 注册 Update 阶段，不碰 ECS 组件）每帧扫 scene_materials，对「`base_color_texture_path` 非空且 Handle 无效」的材质发 `loadAsync` 回填；Prepare 侧新增对称守卫「path 非空 + Handle 无效 → 保持 pending 不 prepare」——否则材质先到、Handle 未回填时会先按白纹理 prepare 且永不重建（`m_materials` 命中即返回），存在顺序竞态卡白。normal/MR 贴图不加载不回填（D4）。V 翻转（D2）：cooker UV 解码处 `v = 1.0f - v` 写入 `.emesh`（注释说明 glTF UV 左上原点 vs OpenGL 底左），纹理 loader 不动，产物全量重跑。管线变体（D3，按 4a 统计收窄）：`double_sided` → `CullMode::None` 手写分支（lamp_glass_01、glass 命中）；`mask` → fs `uAlphaCutoff` discard 分支（MaterialParam 下发，opaque/blend 传 0 关闭；Sponza 无 mask 材质，代码注释标明未验证）；blend 当 opaque（dirt_decal，记 TODO.md）。shade_mode 退役：3c 已移除 demo spawn、无保留方，彻底拆除——`MaterialAsset::shade_mode` 字段、`mat_white`、内联 GLSL 的 `uShadeMode`/`uModel` 白模分支（vs 法线变换 + fs N·L）、`RenderExecuteSystem` 的 `setMat4("uModel")`；fallback 粉色材质保留；TODO.md 原 shade_mode 条目关闭。无材质实体由 scene_loader 用共享默认材质兜底并告警（Sponza 未触发）。
> **验收**：Debug 构建通过；EntelechyTests 207 全绿（基线 207，无新增——改动为装配/渲染路径，与 3c 先例一致不配单测）；lint 全仓零违规。cooker 重跑：405 primitives 全 cook（0 跳过）、warnings 0、28 `.emat` 重生成、统计不变（0 mask / 1 blend / 2 doubleSided / 3 缺 baseColor）。游戏实跑 ~216s（`logs/game_run_4b.log`，窗口收到 Close 事件干净退出）：405 实体 spawn、28 unique materials；回填发出 25 个 baseColor 贴图 loadAsync（28 − 3 factor-only，与 4a 统计吻合）；405 mesh + 25 贴图 + 28 材质全部 prepare 落地（含 2 个 `double_sided=1` 材质）；全部资产就绪后零 pending/fallback 日志（最后一条 pending 在启动期第 457 行，全日志 3357 行）、全程零 ERROR/WARN；216 次秒级 `Frame stats` 全部 `draw_calls == visible`、`total=405`、culled 随视角 120↔405 变化、fps 53–61 无回归。**视觉验收（贴图是否颠倒、glass 双面是否生效、整体观感对照 `_content/sponza/Render_Main_*.png`）待用户目视确认**——引擎仍无截图机制，沿用 3c 方式；相机初始位姿保持朝场景主体未动。

**目标**：窗口中 Sponza 全贴图显示——砖墙、拱门、地砖、旗帜可辨认，与 `_content/sponza/Render_Main_*.png` 官方渲染图布局观感一致；无 fallback 残留。

**范围**：

1. 游戏侧（迁移前最后一版）：`scene_loader` 解析 `material` 路径 → 按路径去重 `loadAsync` `.emat` → 材质内贴图路径 `loadAsync` 回填 → 实体挂各自 `MaterialAssetRef`（替换共享 `mat_white`）。加载中状态由 2c 粉色 fallback 机制覆盖，粉→贴图翻转应终于可视。
2. V 翻转：cooker 按 D2 翻 UV，重跑 cook；实跑确认贴图方向（砖缝、文字、旗帜图案朝向对照官方渲染图）。
3. alpha/doubleSided：按 D3 实现两个管线分支；对照 4a 统计逐一确认 mask 材质（帘幕/格栅）无破洞、doubleSided 材质背面可见。
4. `shade_mode` 临时机制退役：贴图材质走 albedo 路径（`shade_mode=0`），白模开关随 `mat_white` 一并清理（若 demo 仍需要则保留但明确其 demo 属性）。

**验收**：Debug 构建通过；既有测试不红；lint 干净。实跑：全部贴图加载完成后零 pending、零 ERROR/WARN（日志佐证，同 3c 手法）；`Frame stats` 剔除/帧率无回归；**目视验收全贴图**（引擎仍无截图机制，沿用 3c 用户目视确认方式）。贴图颠倒/破洞等异常若出现且 30 分钟内定位不了，按「遇阻即停」汇报。

---

## 4c —— scene_loader 迁引擎 + 法线/MR 落位 + 收尾

**目标**：场景加载入口归引擎；normal/MR 贴图加载落位（为阶段 5 铺路）；文档/债务全同步。

**范围**：

1. `scene_loader` 从 `_game/source/runtime/` 迁入引擎（D5），含 4b 期间加的材质加载逻辑一并带走；`GamePlugin::setup()` 只传场景路径；游戏侧 `render_assets.h` 清理 Sponza 专属内容。按 AGENTS.md 规则更新两个模块的 `AGENTS.md`（游戏侧入口点变更、引擎侧新增模块需新建 `AGENTS.md`）。
2. normal/MR 贴图：`loadAsync` 落位到 `Assets<TextureAsset>`，Handle 回填进 `MaterialAsset`；Prepare 不动（D4）。日志确认全部引用 PNG 加载无失败。
3. 文档同步：`docs/RENDER_LAYER_PROGRESS.md` 5.11/5.13/5.14 章节「代码现状」与完成度；`plan/RENDER_SPONZA_ROADMAP.md` 阶段 4 标记完成；偏差与债务记 `TODO.md`（预期至少含：blend 未实现、手写 JSON 解析器数量、normal/MR 已加载未采样）。

**验收**：Debug 构建通过；全量测试绿；lint 干净；实跑行为与 4b 验收时一致（迁移不引入行为变化）；`game_plugin.cpp` 中场景加载仅剩一行调用。

---

## 跨子步注意事项

- 每子步结束跑 `python scripts/build/build.py --debug` + 全量测试 + `python scripts/tools/lint.py --staged`。
- 每子步完成后更新 `docs/RENDER_LAYER_PROGRESS.md` 对应章节「代码现状」；偏差与新技术债务记入 `TODO.md`。
- **遇阻即停**：V 翻转方向（D2 判断若被实跑推翻）、alpha 破洞定位两处是已知风险点，卡住停下汇报，不绕路、不两侧乱改。
- cook 产物（`.emesh`/`.emat`/`scene.json`）不入 git 的规则沿用 3b，UV 翻转变更后必须重跑 cook 再验收。
- 不做：光照/normal map 采样/MR 解包（阶段 5）、材质 TAI 三层与 PSO 变体系统（阶段 6+）、blend 正确混合与透明排序（随光照阶段再议）。
