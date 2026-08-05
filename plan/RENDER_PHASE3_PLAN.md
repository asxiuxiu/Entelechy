# 阶段 3「看见 Sponza 骨架」详细执行计划

> **生成日期**: 2026-08-05
> **父文档**: [RENDER_SPONZA_ROADMAP.md](RENDER_SPONZA_ROADMAP.md) 阶段 3
> **拆分原因**: 阶段 3 三项工作（mesh 格式 + Loader / glTF cook 工具 / spawn + 白模验收）串行依赖，且 cook 工具与 mesh 文件格式均为无先例的新东西；沿用阶段 2 的拆法，每步独立验收、可独立提交。
> **目标资产**: `_content/sponza/NewSponza_Main_glTF_003.gltf`（+ 134MB .bin；115 meshes / 155 nodes / 28 materials；glTF 2.0，顶点自带 NORMAL/TANGENT，Y-up 右手系与引擎一致）

---

## 现状关键事实（2026-08-05 调研）

- `MeshVertex` 顶点布局**已与 glTF 对齐**（`asset/public/type/mesh_asset.h:18`：`position/normal/uv/tangent + tangentW` 交错 12 floats，注释明确为 glTF TANGENT.w 预留）；`PrepareAssetsSystem` 的属性表 `s_meshAttrs`（`render_system/private/prepare/PrepareAssetsSystem.cpp:41`）已含 tangent。白模/法线着色零管线改动。
- `MeshAsset` 一资产一 primitive（`mesh_asset.h:35`：顶点流 + u32 索引 + AABB），无 sub-mesh——与 roadmap「每 primitive 一实体」正好匹配。
- **mesh 文件格式 / `MeshAssetLoader` 不存在**；`TextureAssetLoader`（`asset/private/loader/texture_asset_loader.cpp`）是现成模板。
- 第三方依赖全部走 Conan（`conanfile.py` 先例：`stb/cci.20230920`）；仓库无 vendor 目录。
- 渲染着色器为 `PrepareAssetsSystem.cpp:19-38` 内联 GLSL（unlit，`uMVP/uColor/uBaseColorTex`）；白模法线着色改此处即可。
- 实体 spawn 模式现成：`_game/source/runtime/private/game_plugin.cpp:66` `GamePlugin::setup()`，实体挂 `Transform/GlobalTransform/MeshAssetRef/MaterialAssetRef/AABB`；资产经 `render_assets.h:61` `initRenderAssets()` 异步加载。
- ECS 有 `SceneSerializer`（`ecs/public/prefab/scene_serializer.h`，JSON 序列化/反序列化），但 `MeshAssetRef` 持运行时 `Handle`（会话局部、不可序列化）——**场景清单必须存 mesh 路径，spawn 时解析为 Handle**，不能直接序列化 World。其 JSON 解析实现（`ecs/private/prefab/scene_serializer.cpp`）可供游戏侧解析清单时参考/复用。
- RHI 无实例化/UBO，数百 primitive 逐 `drawIndexed` + immediate uniform 足够（接口注释自述留待 bindless 再议）。

---

## 决策（已定）

- **D1 glTF 解析库：cgltf（经 Conan）**。知识库检索无 glTF 解析库对比笔记（已搜 cgltf/tinygltf/glTF，无直接条目），roadmap 预推荐 cgltf，工程判断确认：单头文件 C 库、MIT、零依赖，与 stb 引入方式同构（`conanfile.py` 加一行 + `find_package`）；tinygltf 否决（header-only C++ 但拖 json.hpp + stb_image_write，依赖更重）。ConanCenter 包名/版本实现时以 `conan search` 实际可用为准（参照阶段 2b stb 版本踩坑先例）。
- **D2 cook 工具用 C++（用户拍板）**。新建 `_engine/tools/mesh_cooker/` 独立可执行 target（CMake 选项控制，默认开），链接 asset 模块直接复用 `MeshVertex`/`MeshAsset` 定义——**格式单一事实来源，无双端漂移**（Python 路线因此否决）。按 AGENTS.md 规则为该模块新建 `AGENTS.md`。
- **D3 cooked mesh 格式 `.emesh`**：小端二进制，魔数 + 版本 + 顶点数/索引数 + AABB + 原始顶点/索引数组（内存布局即 `MeshVertex`/`u32`，直接 dump）。不做压缩、不做通用性，阶段 6+ 再议。
- **D4 场景清单**：cook 输出 `scene.json`（实体数组：`{mesh: ".emesh 相对路径", transform: 世界变换, material: 材质名占位}`）。cooker 侧手写 JSON emit（格式固定，无需 JSON 库）；游戏侧解析参考 `scene_serializer.cpp` 的既有 JSON 解析。材质字段本阶段仅占位（全部用共享白模材质），阶段 4 填充。

---

## 3a —— mesh 二进制格式 + MeshAssetLoader ✅ 已完成（2026-08-05）

> **落地情况**：格式定于 `asset/public/type/mesh_format.h`（40 字节头：魔数 "EMSH"/版本 1/顶点数/索引数/AABB + 原始顶点/索引 blob，小端无压缩；`writeMeshFile()` writer 与 cook 工具/测试共用，头内 `STATIC_ASSERT` 守护布局）；loader 为 `asset/private/loader/mesh_asset_loader.cpp`，注册方式为 `RenderAssets` 持 `MeshAssetLoader` 实例（AssetServer 无扩展名注册表）。AABB 取「信任文件内值」。无偏差、无新债务。
> **验收**：Debug 构建通过；EntelechyTests 186 全绿（179 基线 + 7 新增：往返/空 mesh/垃圾/截断/错魔数/错版本/无效 FileData）；lint 干净。

**目标**：定稿 `.emesh` 格式，实现 `MeshAssetLoader : IAssetLoader<MeshAsset>`（对照 `TextureAssetLoader` 同构），单元测试覆盖往返读写与垃圾数据拒绝。不引入 cgltf。

**范围**：

1. `asset/` 模块新增格式定义头（魔数/版本/头结构/布局常量）与写出辅助（cook 工具与测试共用的 writer）。
2. `asset/private/loader/mesh_asset_loader.cpp`：读文件 → 校验魔数/版本/长度 → 填充 `MeshAsset`（顶点、索引、AABB；信任 cook 产物的 AABB 或读回后 `computeBounds()` 校验，取简单者）。
3. 测试：内存构造 MeshAsset → writer 写 → loader 读回，断言顶点/索引/AABB 一致；垃圾数据、截断文件、错误魔数拒绝路径。

**验收**：Debug 构建通过；新增测试绿；既有 179 测试不红；lint 干净。

---

## 3b —— glTF cook 工具 ✅ 已完成（2026-08-05）

> **落地情况**：`MeshCooker` 可执行落于 `_engine/tools/mesh_cooker/`（`entelechy_module(TYPE EXECUTABLE)`，链接 AssetLib 复用 `MeshVertex`/`MeshAsset`/`writeMeshFile()` + Conan `cgltf/1.13`）；根 `CMakeLists.txt` 的 launcher 自动链接循环新增 EXECUTABLE 类型跳过（工具 target 不可被链接）。accessor 经 `cgltf_accessor_read_float/read_index` 解码，sparse 告警跳过、缺属性填默认告警、无索引补顺序索引（Sponza 均未触发）；世界变换用 `cgltf_node_transform_world()`（列主序，与 `Mat4::m[16]` 一致）；材质字段写 glTF material name 占位。无偏差、无新债务。
> **验收**：Debug 构建通过；EntelechyTests 186 全绿（基线不变，未新增测试）；lint 干净。cooker 实跑零错误零告警，统计对账：glTF 声明 115 meshes / 155 nodes / 28 materials；405 primitives 全部 cook 成功（405 个 `.emesh`，0 跳过）；`scene.json` 405 实体（本资产无 mesh 跨 node 实例化，实体数 = primitive 数）；抽查 3 个 `.emesh` 头部魔数/计数/长度正确、AABB 有限非零；`scene.json` 可被 JSON 解析、405 个实体均带非空材质名、引用的 mesh 文件全部存在。产物路径 `_content/sponza/cooked/`（meshes/*.emesh + scene.json）。**入库决定：不入 git**——产物可由 cooker 重新生成且体积大，`.gitignore` 现有 `_content/*` 规则已覆盖，无需改动。

**目标**：`mesh_cooker` 可执行：cgltf 解析 `NewSponza_Main_glTF_003.gltf` → 每个唯一 primitive 输出一个 `.emesh`（accessor 解码 + 交错化为 `MeshVertex`）→ 遍历 node 树烘焙世界变换，输出 `scene.json`。产物落 `_content/sponza/cooked/`。

**范围**：

1. `conanfile.py` 增加 cgltf；`_engine/tools/mesh_cooker/` 建 target 先例（链接 AssetLib 复用 3a 的格式定义与 writer）。
2. accessor 解码：POSITION/NORMAL/TEXCOORD_0/TANGENT → 交错 `MeshVertex`；索引 componentType 归一为 u32。**已知风险**：interleaved accessor（byteStride）、sparse accessor、TANGENT 缺失（本资产自带，缺失时填默认值并告警）。
3. node 树遍历：TRS → 世界矩阵（沿父链累积）；同一 mesh 多 node 引用时 `.emesh` 只输出一次，清单按引用计数。
4. 输出统计：mesh/primitive/node 计数与 gltf 头部声明对账（115 meshes / 155 nodes）。
5. 对 NewSponza 实际跑通，产物入库规则同 `_content/demo/`（.gitignore 例外或忽略，实现时定）。

**验收**：cooker 运行零错误；`.emesh` 数量 = gltf primitive 总数；抽查若干 `.emesh` 经 3a loader 读回、AABB 合理（非零、非 inf）；`scene.json` 实体数与 node-mesh 引用数一致。

---

## 3c —— spawn + 白模渲染验收

**目标**：游戏侧读 `scene.json` → 批量 `loadAsync` → spawn 数百实体；白模（法线着色）渲染完整 Sponza，自由相机漫游，剔除统计生效。

**范围**：

1. 场景加载入口（`_game/source/runtime/`）：解析清单 → 每实体 `Transform/GlobalTransform/MeshAssetRef/AABB`（AABB = 世界变换 × mesh.bounds）+ 共享白模 `MaterialAssetRef`；mesh 经 AssetServer 异步加载，fallback 粉色机制（2c 已有）覆盖加载中状态——大资产量下粉→灰翻转应终于可视。
2. 白模着色：`PrepareAssetsSystem.cpp` 内联 GLSL 增加法线着色路径（N·L 或 normal-as-color，取实现最简单者；不动属性表/管线结构）。
3. 相机漫游验证：宫殿内外穿行，ImGui 面板确认剔除数随视角变化、draw call 数与可见实体数吻合。
4. 截图验收，帧率记录为后续优化基线。

**验收**：Debug 构建；Sponza 几何完整可辨认（拱门、柱廊、帘幕）；全部 mesh 加载完成后无 fallback 残留；既有测试不红；更新 `docs/RENDER_LAYER_PROGRESS.md` 5.9/5.5 与相关 `AGENTS.md`，偏差/债务记 `TODO.md`。

---

## 跨子步注意事项

- 每子步结束跑 `python scripts/build/build.py --debug` + 全量测试 + `python scripts/tools/lint.py --staged`。
- 每子步完成后更新 `docs/RENDER_LAYER_PROGRESS.md` 对应章节「代码现状」；偏差与新技术债务记入 `TODO.md`；新增模块（mesh_cooker）按规则新建 `AGENTS.md`。
- **遇阻即停**：cgltf Conan 包可用性、accessor interleaved/sparse 布局两处是已知风险点，卡住停下汇报，不绕路。
- V 翻转（glTF UV 原点左上）阶段 3 不处理——白模不采样贴图，留给阶段 4 暴露并修复。
- 不做：材质/纹理还原（阶段 4）、光照（阶段 5）、BindGroup/instancing 优化（阶段 6+）。
