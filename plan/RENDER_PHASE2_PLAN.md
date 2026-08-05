# 阶段 2「资源进管线」详细执行计划

> **生成日期**: 2026-08-05
> **父文档**: [RENDER_SPONZA_ROADMAP.md](RENDER_SPONZA_ROADMAP.md) 阶段 2
> **拆分原因**: 阶段 2 三项工作（Handle 集成 / 资产类型 + Loader / PrepareAssetsSystem）串行依赖、单会话偏紧；拆为三个可独立验收的子步，每步结束 Debug 构建全绿、可独立提交。

---

## 现状关键事实（2026-08-05 调研）

- `asset/` 模块 Handle 基础设施完整且有测试（`Handle<T>` 8 字节 POD：index+generation；`Assets<T>` 提供 `insert/allocateEmpty/fill/get/remove`；`AssetServer` 单后台线程异步加载 + 主线程 `processEvents()`），但**全工程无生产调用点**。
- 加载状态机 `AssetLoadState` 七态已定义（`asset/public/type/asset_types.h`）但无人使用；「未加载」判定只能靠 `Assets<T>::get()` 返回 nullptr。
- `MeshAsset`/`TextureAsset` **无任何雏形**。
- 裸 `u32 asset_id` 链路共 7 个引擎源文件 + 2 个游戏文件 + `launch/templates/main.cpp.in` 手工注册块。
- GPU 资源创建收敛在 `RenderExecuteSystem::registerMesh/registerColorMaterial`；`GpuMesh`（VBO/IBO/index_count）≈ `PreparedMesh`。
- GPU 资源 fence 延迟回收已在 RHI 层就绪（`GPUResource::release()` + `signalFrame/flushPendingDeletes`），本阶段不需要动。
- 第三方依赖**全部走 Conan**（`conanfile.py`：glfw/glad/imgui/mimalloc），无 vendored 单头文件先例。

---

## 2a —— Handle 集成（断裂 #1）✅ 已完成（2026-08-05，commit `866c7c8`）

> **落地情况**：21 文件 +254/-65，EntelechyTests 169/169 绿，lint 干净，demo 画面用户已确认无回归。D1 反射按预案放弃（记入 TODO.md），D2/D3 按计划落地。

**目标**：`MeshAssetRef`/`MaterialAssetRef`/`RenderMesh`/`RenderMaterial` 的裸 `u32 asset_id` 全部替换为 `Handle<MeshAsset>`/`Handle<MaterialAsset>`；不引入 Prepare，不改加载行为，纯类型迁移。

**范围**（编辑点约 10 处）：

| 文件 | 改动 |
|------|------|
| `render_system/public/components/MeshAssetRef.h` | `u32 asset_id` → `Handle<MeshAsset>` |
| `render_system/public/components/MaterialAssetRef.h` | `u32 asset_id` → `Handle<MaterialAsset>` |
| `render_system/public/components/RenderComponents.h` | `RenderMesh`/`RenderMaterial` 字段同步迁移 |
| `render_system/private/components/component_registration.cpp` | 反射字段适配（见决策 D1） |
| `render_system/private/extract/ExtractRenderablesSystem.cpp` | Extract 搬运 Handle |
| `render_system/private/queue/QueueDrawsSystem.cpp` | SortKey 取值适配（见决策 D2） |
| `render_system/private/execute/RenderExecuteSystem.{h,cpp}` | `HashMap<u32, GpuMesh>` → 以 Handle 为键（见决策 D3） |
| `render_system/CMakeLists.txt` | PUBLIC_DEPS 增加 AssetLib |
| `_game/source/runtime/public/render_assets.h` + `private/game_plugin.cpp` | ID 常量改为经 `Assets<T>::insert` 获得的 Handle |
| `launch/templates/main.cpp.in` | 手工注册块改为先 insert 资产拿 Handle，再注册 GPU 资源 |
| `render_system/tests/test_render_parallel.cpp:83` | `RenderMaterial{u32,...}` 构造适配 |

**前置类型**：`MeshAsset`/`MaterialAsset` 在 2a 只作为 Handle 的模板参数出现，先在 `asset/` 或 `render/` 定义**最小空结构体**占位（字段留给 2b 填充），避免 2a 背负资产格式设计。

**决策（已定）**：

- **D1 反射**：Handle 的 `index`/`generation` 拆为两个 `u32` 字段注册；若反射宏不支持嵌套类型，则这两个字段暂时放弃反射并记入 TODO.md。
- **D2 SortKey**：`material_id` 取 `handle.index & 0xFFFF`，行为与现状一致；2c 引入 PreparedMaterial 后再换 pipeline key。
- **D3 Execute 键**：`registerMesh/registerColorMaterial` 保持手动注册语义（2c 才由 Prepare 取代），键从 `u32` 换成 Handle 的 index（或 `Handle<T>` 本身，取实现更简单者）。

**验收**：Debug 构建通过；`EntelechyTests` 全绿；立方体阵列 demo 渲染与迁移前一致（6×6 阵列、遮挡正确、剔除数随视角变化）。

---

## 2b —— 资产类型 + Loader ✅ 已完成（2026-08-05）

> **落地情况**：EntelechyTests 175/175 绿（新增 6 个：MeshAsset AABB/空网格/顶点步长、PNG 解码、垃圾数据/无效 FileData 拒绝路径）。
> **偏差**：① 计划的 `stb/cci.20230909` 在 ConanCenter 不存在，实际用 `stb/cci.20230920`；② core 无 `Vec4` 类型，`MeshVertex` 切线拆为 `Vec3 tangent + f32 tangentW`（布局仍 12 floats，与 glTF TANGENT 对齐）；③ 标量别名（u8/f32/usize）在全局命名空间而非 `Entelechy::`，首次构建踩坑后修正。
> **新债务**（已记 TODO.md）：`AssetLoadState` 状态机仍无使用方，loader 失败无法与「未加载」区分；`STB_IMAGE_IMPLEMENTATION` 集中于 AssetLib 私有 TU，后续其他模块需用 stb 时应抽公共包装。

**目标**：填充 `MeshAsset`（CPU 侧顶点/索引 + AABB）与 `TextureAsset`（像素数据 + 尺寸/格式），实现对应 `IAssetLoader<T>`；纹理解码引入 stb_image。

**范围**：

1. `MeshAsset`：顶点流（阶段 2 先支持 position/normal/uv/tangent 交错布局，与阶段 3 glTF cook 输出对齐）+ 索引 + AABB。
2. `TextureAsset`：RGBA8 像素 + width/height；`IAssetLoader<TextureAsset>` 用 stb_image 解码 PNG。
3. `IAssetLoader<MeshAsset>`：阶段 2 先支持程序化网格注册路径（游戏侧构造 MeshAsset 直接 `insert`），文件格式 loader 留给阶段 3 cook 产物。
4. **stb_image 走 Conan**（已定）：`conanfile.py` 增加 `stb/cci.20230909`（ConanCenter 现有版本，版本号以 `conan search` 实际可用为准），不开 vendored 先例。
5. loader 单元测试：解码一张小 PNG 断言尺寸/像素；MeshAsset AABB 计算断言。

**验收**：Debug 构建通过；新增测试绿；既有测试不红。

---

## 2c —— PrepareAssetsSystem（断裂 #2）+ 场景验收 ✅ 已完成（2026-08-05）

> **落地情况**：Debug 构建通过，EntelechyTests 178/178 绿（新增 3 个 mesh_primitives 测试）；运行日志确认完整加载状态流转（异步请求 → mesh/材质 prepared → 256×256 纹理上传 → 贴图材质 ready），零错误。
> **关闭崩溃修复**（2026-08-05 后续，同一提交批次）：用户报告关闭窗口时 AV 崩溃，探针定位出两个独立根因——① `HashMap` 槽位生命周期不一致：`clear()` 就地析构不重建、occupied 转换时 construct_at 覆盖存活默认值，与 `~HashMap`/`grow`「每槽位恰好析构一次」的不变量冲突（RAII 值被二次析构，`RHIRef` 二次 release 已释放的 GPUResource）；修复后 `EntelechyTests` 179/179 绿（新增 RAII 探针回归测试 `core/tests/test_containers.cpp`）。② `VFS::clear()` 会对 backend 调 `DefaultAllocator::free`，而游戏侧把挂载点做成 `RenderAssets` 成员对象；改为 `DefaultAllocator::alloc` + `construct_at` 分配（VFS 隐式所有权陷阱已记 TODO.md）。修复后优雅关闭全程干净（日志完整到 "Window closed. Goodbye."）。
> **偏差**：① Prepare 不主动发起 `loadAsync`（Handle 无路径信息），加载由游戏侧启动时发起，与原计划「未加载则触发 loadAsync」不同；② fallback 粉色路径运行时未被触发——demo PNG 太小，后台线程在首帧 `processEvents` 前就完成了解码，可视验证（粉→贴图翻转）留给更大的资产或人为延迟加载；③ 截图验证未完成（执行机锁屏，截图为锁屏界面），改以日志验证；④ 无贴图材质绑 1×1 白纹理而非 shader 变体分支；⑤ VFS 双挂载点兼容项目根 / `build/bin/Debug` 两种 cwd；⑥ `_content/demo/` 加入 git（.gitignore 例外，其余 _content 仍忽略）。
> **遗留观察**：demo exe 被外部 kill，未走正常 shutdown，Prepare 资源的 fence 延迟回收路径未在本次运行中实际执行（代码路径与阶段 1 相同）。

**目标**：新增 Prepare 阶段（插在 Extract 与 Cull 之间），把 Extract 来的 Handle 经 `Assets<T>` 解析为 GPU 资源；`RenderExecuteSystem` 从手工注册表改为消费 Prepared 资源；fallback 机制覆盖加载中状态。

**设计定稿**（2026-08-05 实施前确认）：

- `PrepareAssetsSystem`（`render_system/prepare/`）持有 `handle → PreparedMesh/PreparedMaterial/RHITextureRef` 三张缓存表；设备与 ShaderCache 借用自 Execute（init 时注入），资产存储由游戏侧 `bindAssets()` 注入。
- `MaterialAsset` 填充为 unlit 模型：`base_color`（Vec3）+ 可选 `base_color_texture`（`Handle<TextureAsset>`）。着色器恒采样，无贴图材质绑 1×1 白纹理；fallback = 品红材质 + 单位立方体网格。
- 材质贴图未就绪 → 材质整体保持 pending（粉色），贴图落地后自动替换——即验收要求的「热拔插」可视验证。
- Prepare 只解析 Handle，**不主动发起 loadAsync**（Handle 不含路径信息，路径去重本就是 TODO 债务）；加载由游戏侧启动时发起。与原计划「未加载则触发 loadAsync」有偏差，验收时记录。
- 程序化网格构建器（立方体/地面）放 asset 模块 `mesh_primitives.h`（引擎 fallback 与游戏场景共用）。
- 帧顺序：Extract → **Prepare** → Cull → Queue → Execute；`main.cpp.in` 手工注册块删除，每帧 `AssetServer::processEvents()` 消费异步完成事件。

**范围**：

1. `PrepareAssetsSystem`：维护 handle→「已请求加载」dedup 表（无状态机可用，靠 `Assets<T>::get()==nullptr` 判定未加载）；未加载则 `loadAsync` + 记 fallback；已加载则创建/复用 `PreparedMesh`（VBO/IBO RHIRef，即现 `GpuMesh` 改名/迁移）与 `PreparedMaterial`（管线 + uniform 数据 + 纹理绑定）。
2. `RenderExecuteSystem` 改造：消费 Prepared 资源；`registerMesh/registerColorMaterial` 手工入口删除；`main.cpp.in` 手工注册块整体删除。
3. fallback：粉色材质 + 占位网格；加载完成后自动替换（验证「热拔插」）。
4. demo 场景更新：地面 + 柱子（程序化 MeshAsset），PNG 贴图经 AssetServer 异步加载。
5. 日志可见加载状态流转；卸载路径经 fence 延迟回收（RHI 层已就绪，只需验证）。

**验收**：Debug 构建；带贴图物体正确显示；未加载→加载完成过程中 fallback 粉色被正确替换；`test_gpu_resource_lifecycle` 等全部测试绿。

---

## 跨子步注意事项

- 每子步结束跑 `python scripts/build/build.py --debug` + 全量测试。
- 每子步完成后更新 `docs/RENDER_LAYER_PROGRESS.md` 5.8/5.14 的「代码现状」，偏差与债务记入 `TODO.md`。
- SortKey 换 pipeline key、`Changed<T>` 同步、`AssetLoadState` 状态机接入均**不在本阶段**，记入 TODO.md 留给阶段 6+。
- 遇阻即停：Handle 反射适配、stb_image Conan 包可用性两处若卡住，停下汇报，不绕路。
