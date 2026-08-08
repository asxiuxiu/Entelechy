# SelfGameEngine TODO / 技术债务

## ECS / Core Runtime
- [ ] ECS / Core Runtime | `src/world.h` 中 `std::vector<bool> alive` 存在位压缩特化性能陷阱，读写较慢且无法返回真实引用，需替换为 `std::vector<uint8_t>` 或引入 Sparse Set 架构。
- [ ] ECS / Core Runtime | `src/world.h` 中 `positions` / `velocities` 数组只增不减，先 spawn 大量实体再销毁后数组仍保持峰值大小，浪费内存，需升级为 Sparse Set（`dense + sparse`）让存活数据保持紧凑。
- [ ] ECS / Core Runtime | `AgentBridge`（`_engine/source/bridge/agent_bridge.cpp:12`）越界承担 System 注册职责，直接持有 `World`、`Scheduler`、`MovementSystem` 并在 `init()` 中注册，把「AI 接口桥梁」与「运行时装配」耦合在一起，需将装配逻辑上提到 Runtime / App 层，`AgentBridge` 仅通过指针/引用访问外部注入的世界与调度器。
- [ ] ECS / Core Runtime | `ecs/type/type_registry.h` `allocateNextID()` 使用 `u64` 掩码硬限制 64 种组件类型（`CHECK(m_next_id < 64)`），当前约 10~15 个但渲染/物理/动画组件加入后将触顶，需迁移到 Archetype/Chunk 存储或改用 `DynamicArray<u64>` 位集。
- [ ] ECS / Core Runtime | `ecs/world/world.h` `m_component_arrays` 使用 `HashMap<ComponentTypeID, IComponentArray*>` 裸指针存储，无分配器注入，生命周期由 `World` 析构手动 `delete`。
- [ ] ECS / Core Runtime | ECS 当前没有 Resource 概念（全局单例数据的调度器感知存储）。`String` / `Assets<T>` / `InputState` 等全局数据目前以独立单例或全局变量形式存在。Resource 的核心价值不是存储，而是**让 Scheduler 感知 System 对全局数据的读写依赖以正确并行调度**。需在 System 并行化前引入 `insertResource<T>()` / `Res<T>` / `ResMut<T>` SystemParam。注意：并非所有全局状态都应进 ECS Resource——仅被多个 ECS System 并发访问且调度器需要感知依赖的数据才需要（如 `Time`、`InputState`、`Assets<T>`），底层基础服务（如 `StringInternPool`、`IAllocator`）保持单例即可（2026-08-04 讨论结论）。
- [ ] ECS / Core Runtime | `ecs/world/plugin.h` `PluginManifest` 只记录 name/phase/dependencies，缺少 registered_components / registered_systems / registered_resources，AI Agent 无法完整查询插件能力。

- [x] ECS / Core Runtime | `ThreadPool/Submit1000Tasks` 测试偶发卡死/段错误（2026-08-07 复现，~25% 失败率）。根因（2026-08-08 定位）：(A) `submit()` 从任意线程直推 worker 本地 Chase-Lev 队列，违反 `push()` owner-only 约束——外部 push 与 owner `pop()` 在 `m_bottom` 上无同步竞争，导致任务丢失与 `std::function` 槽位撕扯（`bad_function_call`/野指针）；(B) 构造函数边建 Worker 边启动线程，worker steal 遍历 `m_workers` 时主线程 `pushBack` realloc 造成 use-after-free。修复：外部提交一律走全局 mutex 队列 + 构造两阶段化；新增 `StressDiagnostic` 回归测试（300 轮 submit/wait + 逐任务执行计数 + 有界等待）。
- [ ] ECS / Core Runtime | ThreadPool 本地工作窃取队列当前无生产路径（外部 submit 全走全局 mutex 队列），`WorkStealingQueue` 的 push/pop/steal 实质空转。引入 worker 本地任务派生（嵌套并行）时需 TLS owner 路由或 MPSC 安全的外部 push，并回归 `StressDiagnostic`。

## Asset / 资源管理
> 2026-05-25：已完成简化路径（Handle Table + 单后台线程异步加载 + 手动 unload）。以下差额在后续阶段补齐。

- [ ] Asset / 资源管理 | `AssetServer` 使用单 `std::thread` + `std::mutex` 双队列，并发加载 > 5 个时成为瓶颈，需接入现有 `ThreadPool`（工作窃取）并替换 `std::mutex` 为 lock-free MPSC ring buffer（如自研 `ConcurrentQueue`）。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/资源管理.md` 问题 3 分支 C。
- [ ] Asset / 资源管理 | 当前资源无自动依赖解析（如 Material 引用的 Texture 需手动保证加载顺序），需实现 `LoadingGraph`（节点 = 资源，边 = 依赖），反向传播完成事件，依赖全部就绪后触发 `postprocess` 解析内部 `Handle<T>` 引用。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/资源管理.md` 问题 4 分支 C。
- [ ] Asset / 资源管理 | `unload()` 为显式调用，易遗漏，大规模 ECS 场景需自动生命周期管理，需实现 `RefCountUpdateSystem` 每帧批量扫描所有持有 `Handle<T>` 的组件，更新平行引用计数表，零引用槽位在帧边界 `flush_pending_frees()` 统一回收，保证渲染安全。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/资源管理.md` 问题 2 分支 B。
- [ ] Asset / 资源管理 | 热重载只能手动 `reload()` 触发，开发期效率低，需引入 `FileWatcher`（Windows `ReadDirectoryChangesW` / Linux `inotify`），文件变更时推入脏 Handle 队列，帧边界 `HotReloadSystem` 统一异步重新加载并原地替换数据（Handle 不变，引用不断裂）。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/资源管理.md` 问题 6。
- [ ] Asset / 资源管理 | 引擎直接加载原始格式（PNG/FBX），运行时解码慢、内存峰值高、无法利用 GPU 压缩格式，需建立 `AssetImporter` + `ResourceCooker` 工具链，源资产（`SourceAssets/`）经导入/压缩/平台适配后输出运行时资产（`CookOutput/`），引擎只加载 Cook 后的二进制格式。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/资源管理.md` 问题 7。
- [ ] Asset / 资源管理 | 同一路径多次 `loadAsync()` 会创建多个 Handle，造成内存重复和引用计数分散，需 `AssetServer` 维护 `HashMap<Path, ErasedHandle> m_pathToHandle`，加载前先查缓存，已加载直接返回已有 Handle 并 `incrementRef()`。
- [ ] Asset / 资源管理 | `AssetLoadState` 七态枚举（`asset/public/type/asset_types.h`）已定义但全工程无使用方。`TextureAssetLoader`（2b）解码失败仅返回空 `TextureAsset` + 错误日志，调用方无法区分「未加载 / 加载中 / 失败」，阶段 2c 的 fallback 与 dedup 只能靠 `Assets<T>::get()==nullptr` 判定。需在 Prepare/状态机接入阶段把加载状态（含 Failed）落到存储侧。
- [ ] Asset / 资源管理 | `MaterialAsset`（`asset/public/type/material_asset.h`）贴图路径字符串（`base_color_texture_path` 等三个 `String`）与 `Handle<TextureAsset>` 字段双存：loader 按 D1 只解析路径、Handle 由场景加载侧 loadAsync 回填（4b/4c），双存为过渡形态；材质加载链路稳定后应收敛为单一表示（如 Handle 统一经路径缓存解析，或路径字段在 Handle 回填完成后清除）。
- [ ] Asset / 资源管理 | `SceneLoader::backfillMaterialTextures()`（`asset/scene/private/scene_loader.cpp`，4c 随场景加载迁引擎）每帧轮询场景材质清单、对「path 非空且 Handle 无效」的材质补发贴图 `loadAsync`，是 4b 起的过渡机制（`Assets<T>` 不支持遍历，只能另存 Handle 清单）；4c 保持轮询不变，后续应改为加载完成事件驱动或一次性同步解析。
- [ ] Asset / 资源管理 | `STB_IMAGE_IMPLEMENTATION` 目前定义在 AssetLib 私有 TU（`asset/private/loader/texture_asset_loader.cpp`）。未来其他模块（编辑器缩略图、字体图集等）若直接使用 stb_image 会重复定义实现符号，届时应抽独立 stb 包装模块或将实现集中到唯一的 stb TU。
- [ ] Asset / 资源管理 | `HandleTableSlot<T>` 使用 `DynamicArray`，resize 时默认构造元素，要求 T 必须可默认构造，需重构为手动内存管理（`alignas(T) char buffer[sizeof(T)]` + placement new），类似 `std::optional` 或 `Column<T>` 的底层实现。
- [ ] Asset / 资源管理 | AI Agent 需要自描述地了解「当前加载了哪些资源、各占多少内存、引用关系如何」，需通过反射系统注册资源类型 Schema，`AssetServer` 暴露 `query_asset(handle)` / `dump_ref_graph()` 等 MCP 工具。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/资源管理.md` 问题 8。

## Render / RHI
> 2026-05-31：已完成级别 1 轻量优化（调试标注 + Uniform Cache + 错误码骨架）。

- [ ] Render / RHI | `GLRHIDevice` 未包装为 ECS `Resource`，`MeshRenderSystem` 仍直接调用 GL 裸接口，需让渲染系统通过 `IRHICommandList` 录制命令。
  - 参考：`_engine/source/render/rhi_device.h`
- [ ] Render / RHI | 当 Draw Call > 2000 且 Profiling 确认 CPU 瓶颈时，`GLCommandList` 的即时执行模式无法扩展，需引入简化版延迟命令缓冲（`LinearAllocator` + 引擎级命令枚举 + `switch-case` 翻译执行），单线程录制 → 多线程并行生成 `DrawPacket` → 单线程排序 → 单线程翻译。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 2/3。
- [x] Render / RHI | CPU 删除纹理但 GPU 还在读的 race condition，已引入 RHI 内部延迟删除队列，引用计数归零后进入队列，帧边界检查 GPU Fence，确认 GPU 完成后批量释放。
  - 完成：2026-06-19，见 `render/public/rhi/rhi_resources.h`、`render/public/rhi/rhi_device.h`、`render/private/rhi/gl_rhi_device.cpp`。
- [x] Render / RHI | 增加 GPU 显存预算跟踪（`RHIMemoryInfo`、`GLRHIDevice::queryMemoryInfo`、`getTrackedMemoryUsage`）与瞬态纹理池（`TransientTexturePool`）。
  - 完成：2026-06-19，见 `render/public/rhi/rhi_types.h`、`render/public/rhi/rhi_transient_resource_pool.h/.cpp`。
- [ ] Render / RHI | `GLRHIDevice` 目前仍不是 ECS `Resource`，预算/删除队列/瞬态池等状态仍挂在设备单例上。后续 ECS Resource 系统就绪后，应将这些可变状态迁移为 World Resource，仅保留不可变底层后端指针为单例。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与画面/GPU资源生命周期管理.md` 问题 5 分支 C。
- [ ] Render / RHI | 瞬态纹理池目前只按整纹理复用，未实现 RenderGraph 驱动的显存别名（Memory Aliasing）。RenderGraph 成熟后应在 `TransientTexturePool` 下层接入别名分配器。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与画面/GPU资源生命周期管理.md` 问题 4 分支 C。
- [ ] Render / RHI | `PSOManager::getOrCreateAsync()` 缓存未命中时同步编译造成帧时间尖刺（hitch），需返回占位 PSO（如最简单的纯色着色器），同时启动后台线程编译，后台完成后自动切换。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 5。
- [ ] Render / RHI | 纹理显存无预算控制：Sponza 73 张 PNG 全部以 RGBA8 + 完整 mip 链上传（5b 修复 minification aliasing 引入 mip 链后约 +33%），合计约 6.2 GiB 量级。无纹理压缩（BC7/ASTC）、无 cook 期降采样、无流式加载。当前场景能跑，但第二个更大场景或低端 GPU 会爆显存；压缩纹理格式支持需 RHI `TextureDesc` 增加压缩 format 枚举 + cooker 侧编码。
- [x] Render / RHI | RHI 抽象接口目前仅有 OpenGL 后端，需在接口已跑通带纹理旋转立方体、接口稳定后启动第二个后端，先钉死 D3D12（Windows 默认，调试工具顶尖），跑通全部上层管线后再移植 Vulkan。
  - 完成：2026-08-07（阶段 6d），`D3D12RHIDevice` + `D3D12CommandTranslator` 落地，Sponza 画面与 GL 功能等价（截图验证），`--backend=d3d12` / `ENTELECHY_BACKEND` 选择后端。见 `render/public/rhi/d3d12_rhi_device.h`、`render/public/rhi/d3d12_command_translator.h`。Vulkan 预留在 6f。
- [ ] Render / RHI | Render/D3D12MipGeneration：D3D12 后端纹理只建 mip 0（`d3d12_rhi_device.cpp` `createTexture` 强制 `MipLevels=1`，静态 sampler clamp LOD=0），GL 侧有 `glGenerateMipmap` 而 D3D12 无等价 API，远景 minification 会 aliasing（法线贴图最明显）。需 mip 生成 pass（compute box-filter 或简单逐层 blit shader），或 cook 期预生成 mip 链进资产。
- [ ] Render / RHI | Render/D3D12SyncUpload：D3D12 `createBuffer`/`createTexture` 每次上传单独 `CreateCommittedResource` staging + close/execute/fence 同步等待，137 张纹理加载期明显慢于 GL（14000 帧仅 52 张 vs GL 90s 全量）。应改批量上传（staging 池 + 单 list 多 copy + 一次 fence），或异步上传线程。
- [ ] Render / RHI | Render/D3D12PixMarkers：D3D12 翻译器的 `pushDebugGroup`/`popDebugGroup`/`insertDebugMarker` 为空操作（未链接 WinPixEventRuntime），PIX capture 时无事件分层。接入 PIX runtime 后映射到 `PIXBeginEvent`/`PIXEndEvent`/`PIXSetMarker`。
- [ ] Render / RHI | Render/D3D12NoImGui：D3D12 模式下 ImGui 整体跳过（`main.cpp.in` 按后端分支），调试面板/统计叠加层不可用（统计仍有每秒日志）。需接 imgui_impl_dx12（共享 SRV descriptor heap + 独立 render pass），或做后端无关的 UI 叠加抽象。
- [ ] Render / RHI | `D3D12RHIDevice` 的 attribute location→HLSL 语义名映射（0=POSITION/1=NORMAL/2=TEXCOORD/3=TANGENT）硬编码在 `getSemanticName()`，与 `prepare_assets_system.cpp` 的 `s_meshAttrs` 隐式耦合；顶点布局格式升级（如多 UV/顶点色）时两处需同步。6e/材质变体阶段应让语义名直接进入 `VertexAttributeDesc`。
- [ ] Render / RHI | 主 World 与渲染未分离，无法支持 CPU-GPU 流水线并行、多视图隔离、确定性快照，需实现双 World 模型（主 World 跑逻辑 30Hz，Render World 跑渲染 60Hz），每帧 Extract 阶段单向只读复制可渲染数据到 Render World，Render World 内 System 生成命令，最终 `PresentSystem` 输出。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 6。
- [x] Render / RHI | `FrustumCullSystem`（`render/culling/FrustumCullSystem.cpp:37`）与 `QueueDrawsSystem`（`render/queue/QueueDrawsSystem.cpp:31`）每帧通过 `Query<ViewVisibleList>` / `Query<ViewBinnedPhases>` / `Query<ViewSortedPhases>` 遍历全部实体来定位唯一的 Resource-like 组件，且不存在时会创建新实体，导致 render world 实体数只增不减，需将 View-level Resource 绑定到 view 实体或引入 ECS 的 Resource 系统（非组件的全局表）。
  - 完成：2026-06-12，`ExtractCameraSystem` 创建 view 实体时同时预绑定 `ViewVisibleList` / `ViewBinnedPhases` / `ViewSortedPhases`；`FrustumCullSystem` 与 `QueueDrawsSystem` 改为在 view 实体上 O(1) 查取。新增 `RenderViewResources` 单元测试验证每帧仅剩 1 个 view 实体。
- [x] Render / RHI | `BinnedRenderPhase`（`render/queue/BinnedRenderPhase.h`）使用 `DynamicArray<PhaseBin>` 线性分箱，因 ECS `ComponentArray::set` 要求组件可拷贝而 `HashMap` 显式 delete 了拷贝，为绕过限制改用线性查找，大规模场景下退化为 O(n)，需恢复 `HashMap` 或引入独立 Resource 系统。
  - 完成：2026-06-12，`BinnedRenderPhase` 内部改为 `HashMap<u32, usize>` 索引 `DynamicArray<PhaseBin>`，addItem 从 O(n) 降到 O(1) 均摊；同时扩展 `ComponentArray`/`World` 支持 move-only 组件，使包含 `HashMap` 的 `ViewBinnedPhases` 可作为 ECS 组件使用。新增 `BinnedRenderPhase` 单元测试覆盖按 material 分箱、首次出现顺序与 clear。
- [ ] Render / RHI | `QueueDrawsSystem`（`render/queue/QueueDrawsSystem.cpp:65`）深度计算使用物体原点 `worldMatrix * Vec3{0,0,0}` 的 z 值，对于中心偏移的大型物体排序结果不准确，若实体有 `AABB` 应使用 `(aabb.min + aabb.max) * 0.5f` 作为深度采样点，无 AABB 则回退到原点。
- [x] Render / RHI | `QueueDrawsSystem`（`render/queue/QueueDrawsSystem.cpp:48`）SortKey 的 float→uint 编码未处理 viewZ ≤ 0，IEEE-754 负数 bit pattern 按 uint 排序与数值排序不一致，若物体在相机后方或近平面处深度键会混乱，需将 viewZ 钳制到 `[near, far]` 后规范化到 `[0, 1]` 再编码为 uint（如 `uint32_t(depth * 0xFFFFFFFF)`），保证全范围单调。
  - 完成：2026-06-12，`ExtractedView` 增加 `near_plane` / `far_plane`；`QueueDrawsSystem` 改用 `encodeLinearDepth` 将 viewZ 规范化到 `[0, 1]` 后编码为单调 uint；透明/UI 仍通过按位取反实现远→近排序。新增 `SortKeyDepthEncoding` 单元测试覆盖负深度钳制、近平面/远平面边界、单调性与透明降序。
- [ ] Render / RHI | `FrustumCullSystem` 和 `QueueDrawsSystem` 由调用方手动按顺序调用，无内建依赖声明，未来渲染步骤增多后容易顺序出错，需仿照 `ExtractSchedule` 引入 `RenderSchedule`（`IRenderSystem` 接口 + 注册表），在 `RenderWorld` 中统一定义 Extract → Cull → Queue → ... 的 SystemSet 链。
- [x] Render / RHI | `AABB`（`core/math/aabb.h`）未注册为 ECS 组件，`render/tests/test_render_parallel.cpp` 为构造测试场景只能手动调用 `TypeRegistry::registerComponent`。需按模块边界规则在 `render/components/` 下新增 `WorldAABB` / `RenderAABB` 包装组件并注册 `REFLECT_COMPONENT`，`FrustumCullSystem` 应读取该包装组件而非直接使用 `AABB`。
  - 完成：2026-08-05，新增 `render_system/public/components/WorldAabb.h`（主世界 `WorldAABB`）与 `RenderComponents.h` 内 `RenderAABB`（渲染世界），均在 `component_registration.cpp` 注册；`ExtractRenderablesSystem` 搬运 `WorldAABB`→`RenderAABB`，`FrustumCullSystem` 改读 `RenderAABB`；游戏侧 `registerAabbComponent()` 补丁与测试侧手动注册均已删除。
- [ ] Render / RHI | `FrustumCullSystem` / `QueueDrawsSystem` 的并行路径目前依赖调用方传入 `ThreadPool*` 并自行按 batch 拆分/等待，与 `ThreadPool::parallelFor` 不兼容且重复了任务分发样板代码。未来应在 `ThreadPool` 中增加 `parallelForRanges` 或 `parallelBatch` 工具，或让 ECS Scheduler 的 System 级并行调度接管这些系统。
- [ ] Render / RHI | Culling 与 Queue 系统仅处理第一个 `ExtractedView`。`ViewVisibleList`、`ViewBinnedPhases`、`ViewSortedPhases` 已绑定到 view 实体（单视图完成）；多视图扩展时需遍历所有含 `ExtractedView` 的实体，为每个 view 生成独立的可见列表和 phase 容器。
- [ ] Render / RHI | `PhaseItem::instance_count`（`render/queue/PhaseItem.h`、`render/queue/QueueDrawsSystem.cpp`）字段已预留为 1 但未实现 instancing 合并，`QueueDrawsSystem` 未检测同 material + 同 mesh 的连续实体，需在 `BinnedRenderPhase::addItem` 中检测并合并为同一 `PhaseItem` 且累加 `instance_count`，并配合 Prepare 步骤生成 instance buffer。
- [ ] Render / RHI | `ExtractRenderablesSystem`（`render/extract/ExtractRenderablesSystem.cpp:23`）每帧对静态包围盒全量拷贝，大多数模型的本地 AABB 是静态的但每帧都通过 `mainWorld.getComponent<WorldAABB>(entity)` 提取到 render world，需在 `MainWorldSync` 中记录「上一帧是否有 AABB」或引入脏标记机制，仅当 AABB 组件被修改时才重新提取。
- [x] Render / RHI | `IRHICommandList::setUniform*` 为 OpenGL immediate mode（`glUniform*`），每 Draw Call 单独调用驱动。
  - 完成：2026-08-08（阶段 6e）。材质路径改为 `ConstantBufferRing` + `bindConstantBuffer`（GL `glBindBufferRange` UBO / D3D12 root CBV），`setUniform*` 命令保留为遗留序列化（Material 不再产生）。
- [ ] Render / RHI | `RenderExecuteSystem`（`render_system/private/execute/RenderExecuteSystem.cpp`）自持第二个 `GLRHIDevice` + `ShaderCache`（与 `render/example/simple_cube_renderer.h` 同款债务，同源），两个设备实例并存于主循环，需统一为由帧驱动层注入或 ECS Resource 化的单一设备。
- [ ] Render / RHI | 阶段1 网格/材质由 `launch/templates/main.cpp.in` 手工注册（2c 已移除）：~~asset → GPU 资源解析散落在主循环~~ 已由 2c `PrepareAssetsSystem` 接管（`render_system/prepare/`）。残留简化：Prepare 不主动发起 `loadAsync`（Handle 无路径，加载由游戏侧发起），且每帧全量扫描 RenderMesh/RenderMaterial 组件（无 `Changed<T>` 增量），实体规模上量后需增量化。
- [x] Render / RHI | `render_system/private/prepare/PrepareAssetsSystem.cpp` 白模法线着色用 `mat3(uModel)` 直接变换法线，未使用 inverse-transpose 法线矩阵，非均匀缩放下法线方向错误（Sponza 变换为刚体/均匀缩放，阶段 3c 安全）；引入非均匀缩放资产或阶段 5 正式光照前需改为法线矩阵。
  - 完成：2026-08-06（阶段 5a），`uNormalMatrix`（`Mat3::normalMatrix` 逆转置）随每 draw 下发，vs 法线走法线矩阵。
- [ ] Render / RHI | `DirectionalLight.ambient`/`uAmbient`（`render_system/public/components/DirectionalLight.h`、`render_system/private/prepare/PrepareAssetsSystem.cpp` 内联 fs）为常量环境项 ambient×albedo，无 IBL/半球环境光，背阴面缺少天空/地面色变化，引入 IBL 或半球环境光后替换。
- [ ] Render / RHI | `RenderExecuteSystem::drawItem` 每 draw 上传视图/光照/对象常量（`uViewPos`/`uLightDir`/`uLightColor`/`uMVP`/`uModel`/`uNormalMatrix`），且每 draw 在 CPU 侧重算 Mat3 逆转置法线矩阵；6e 后已改走 ring 一次 memcpy，但仍是每 draw 全量写入。视图/光照级数据应按更新频率分层移出逐 draw 路径，法线矩阵可在 Extract 期预计算或按实体缓存。
- [ ] Render / RHI | 天空为 `RenderExecuteSystem` 自持的渐变全屏 pass（内联 sky shader + 全屏三角形，阶段 5c D6），非天空盒；Sponza 资产包不含天空盒贴图（roadmap 明确另行准备），引入天空盒/IBL 时替换渐变 pass，天空颜色目前经 `SkySettings` 组件 + ImGui 调节。
- [ ] Render / RHI | 固定步长 Scheduler 热身期：启动后首个累加周期内 `TransformPropagationSystem` 尚未执行，所有 `GlobalTransform` 仍是零矩阵（首帧渲染为空），且 `GlobalTransform` 默认零矩阵而非单位矩阵放大了该问题。需在 spawn 后强制一次传播、或让 `GlobalTransform` 默认为单位矩阵、或首帧 `tickOnce`。
- [ ] Core / String | `_sid` 字面量是 consteval 纯哈希、不进驻留池，任何经 `StringInternPool::resolve` 反查字符串的消费者（如 `GLCommandList::getUniformLocation`）对未驻留 id 会**静默失败**（2026-08-04 因此导致 Material uniform 全部未上传、画面只剩清屏色，已通过 `MaterialParamDesc` 改 `const char*` + init 时 intern 修复）。后续新增 resolve 消费者时需确保上游驻留，或考虑为 resolve 失败路径加日志/断言。

## Material / Shader
> 2026-05-25：当前实现为同步编译 + CPU uniform 块 + `glUniform*` 即时上传 + 无模板分层。以下为与工业级方案的差额，后续逐步补齐。
> 2026-08-07（6c）：离线着色器编译工具链已落地（HLSL→DXC→DXIL/SPIR-V→SPIRV-Cross→GLSL），运行时从预编译字节码加载。但 SPIRV-Cross 展平后 uniform 名为数组索引形式（type_PerFrame[N]），材质参数表需手动匹配，在 6e UBO/CBV 统一绑定层中彻底解决。combined sampler 命名问题已修复（2026-08-07）：shader_compiler 在 `build_combined_image_samplers` 后把合并采样器重命名回原始 HLSL 贴图名（uBaseColorTex 等），C++ 按原名绑定。同日另修复两个 6c 引入的渲染回归：(1) SPIRV-Cross GLSL 330 输出不带 stage 接口 location，VS 输出 `out_var_*` 与 FS 输入 `in_var_*` 按名链接永远失配，所有 FS 输入读零（贴图/法线/天空全失效）——已将 GLSL 目标版本升至 410 以产出显式 `layout(location)`；(2) sky VS/PS 的 cbuffer 同名 `PerFrame`，展平后同名不同长互相覆盖——PS 侧改名 `PerFramePS`。

- [x] Material / Shader | 6c SPIRV-Cross 降级 GLSL 的 uniform 命名不保留原始 HLSL 成员名（展平为 type_PerFrame[N] 数组），材质参数表硬编码匹配脆弱。
  - 完成：2026-08-08（阶段 6e）。shader_compiler 停止展平 cbuffer（保持真 UBO，`layout(binding=N, std140)` + 420pack 扩展），输出 `_reflection.json`（cbuffer 成员 name/type/offset/size + 纹理 t-register）；运行时 `ShaderReflection` 解析，Material 参数表用真实 HLSL 成员名（`uViewPos`/`uMVP` 等），BindGroup 按绑定点声明式绑定。~~combined sampler 自动命名（_221 等）~~已修复（2026-08-07，编译器重命名回原始贴图名）。详见 `plan/RENDER_SPONZA_ROADMAP.md` 6e 第 5 条。
- [ ] Material / Shader | `Material::initFromBytecode()` 绕过了 `ShaderCache`，每个材质各自 createShader + link 一份相同内容的 GL program（Sponza 28 材质 → 28 份 PSO；6e 的 PSO 缓存按 desc 哈希去重，但 desc 含 shader 指针，同源材质不共享）。显存与 link 时间浪费，应在 ShaderCache 支持字节码哈希去重后解决。
- [x] Material / Shader | 每 draw call 遍历参数调用 `glUniform*`，CPU 开销大且无法合批，需定义 `BindGroupLayout` 并按类型分池，`MaterialBindGroupAllocator` 管理，`CPU uniform` 块一次性写入 GPU UBO，`bind()` 只切换 UBO offset 或 PushConstants。
  - 完成：2026-08-08（阶段 6e）。`BindGroupLayout`/`BindGroup` 已落地（`render/binding/bind_group.h`），`Material::bind(cmdList, ring)` 一次 memcpy CPU blob → ConstantBufferRing + BindGroup 绑定。**剩余**：`MaterialBindGroupAllocator` 池化与按更新频率分层（View/Light per-frame、Material per-material 复用、Object per-draw 动态偏移）——当前 ring 每 draw 全量分配，见下文 6e 遗留项。
- [ ] Render / RHI | **6e 遗留**：`ConstantBufferRing` 每 draw 全量分配 3 个 cbuffer 块（View/Light、Material、Object）。按 roadmap 6e 第 4 条，View/Light 应每帧写一次、Material BindGroup 按材质复用、Object 走动态偏移——需引入按更新频率分层的 ring 段或持久 per-material UBO 后再优化。
- [ ] Render / RHI | **GL 退出时 glad 1282 错误（既有，6e 观测确认）**：teardown 顺序 `window->destroy()` 先销毁 GL 上下文，设备析构时 `GLRHIDevice::shutdown()` 的 pending-delete flush 在死上下文上执行，每个 `glDelete*` 报 1282（进程退出时无害但刷屏）。修复：在 `window->destroy()` 前显式 `flushPendingDeletes()`（或调整 teardown 顺序/设备析构时机）。
  - 完成：2026-08-08（阶段 6e）。`BindGroupLayout`/`BindGroup` 已落地（`render/binding/bind_group.h`），`Material::bind(cmdList, ring)` 一次 memcpy CPU blob → ConstantBufferRing + BindGroup 绑定。**剩余**：`MaterialBindGroupAllocator` 池化与按更新频率分层（View/Light per-frame、Material per-material 复用、Object per-draw 动态偏移）——当前 ring 每 draw 全量分配，见下文 6e 遗留项。
- [ ] Render / 加载性能 | Sponza 全量贴图常驻约需 90 秒（约 3 秒/材质的节奏热替换粉色 fallback），期间画面大面积粉色。纹理解码/上传管线未并行化，影响迭代体验；可考虑并行 decode、预 cook 成 DDS/KTX2 或降低加载分辨率。
- [ ] Material / Shader | 当前 `Material` 直接硬编码 VS/FS，无变体管理，美术资产与 GPU 状态未解耦，需引入模板-实例三层：`ShaderTemplate` → `MaterialAsset` → `MaterialInstance`，`ShaderTemplate` 定义 `Category/Keyword` 变体规则，`MaterialAsset` 存储参数/PassHint/RenderState，`MaterialInstance` 运行时维护 `TechniqueCache[permutation_id]`。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 1 分支 B。
- [ ] Material / Shader | `Material::init()` 同步编译，加载新材质时阻塞渲染线程，需引入 `TechniqueState` 状态机（Invalid → Pending → Valid），缓存未命中时返回预编译 Fallback（如粉色棋盘格），后台线程编译完成后自动切换。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 4 分支 B。
- [ ] Material / Shader | 每 draw call 遍历参数调用 `glUniform*`，CPU 开销大且无法合批，需定义 `BindGroupLayout` 并按类型分池，`MaterialBindGroupAllocator` 管理，`CPU uniform` 块一次性写入 GPU UBO，`bind()` 只切换 UBO offset 或 PushConstants。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 3 分支 C。
- [ ] Material / Shader | 当前 `Material` 为纯 C++ 对象未接入 ECS，渲染系统无法批量处理同材质实体，需引入 ECS 组件 `MaterialAssetRef { AssetId material; }`，渲染架构分 Extract → Prepare → Queue → Render，`PrepareMaterialSystem` 按类型批量创建 `BindGroup`，`QueueOpaqueDraws` 按（材质类型, 管线 key）分组排序。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 5 分支 B。
- [ ] Material / Shader | 当前一材质一 PSO，无法表达「有法线/无法线」「有骨骼/无骨骼」等组合变体，需 `ShaderTemplate` 维护 `Array<ShaderCategory>`，`MaterialInstance::getTechnique(keywords)` 生成 `permutation_id` 并查找/触发编译，严格限制维度（≤5 个二值开关）防止组合爆炸。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 2 分支 C。
- [ ] Material / Shader | 当前 Material 只绑定单个 PSO，无 PassHint 概念，需 `MaterialAsset` 增加 `PassHint`（Deferred/Forward/Translucent/UI），渲染系统按 Pass 分批提取对应 Technique，同一材质在不同 Pass 使用不同 shader 组合。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 工业级设计清单。
- [ ] Material / Shader | 当前材质只能通过 C++ 代码定义，美术无法直观调整，需运行时解析节点图定义 → 生成 GLSL/HLSL 源码 → 通过 `ShaderCache` 编译，编辑器通过 MCP/反射接口让 AI 也能操作节点参数。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 1 分支 C。
- [ ] Material / Shader | `MaterialAssetRef`（`render/components/MaterialAssetRef.h`）缺少 `render_phase` 信息，`ExtractRenderablesSystem`（`render/extract/ExtractRenderablesSystem.cpp:20`）无法推断 phase，全部默认 `Opaque3D`，透明材质被错误分箱到 `BinnedRenderPhase`，需在材质系统（`Material` / `MaterialAsset`）增加 `RenderPhase` 声明，`ExtractRenderablesSystem` 从材质元数据读取 phase。
- [ ] Material / Shader | `PrepareAssetsSystem.cpp` 的 `prepareMaterial` 把 `AlphaMode::Blend` 当 opaque 处理（目前仅 dirt_decal 一个材质），正确混合需要透明排序 + 独立 translucent pass，随光照阶段（阶段 5+）再议。
- [ ] Material / Shader | `PrepareAssetsSystem.cpp` 内联 GLSL 的 `uAlphaCutoff` discard 分支（`AlphaMode::Mask`）未经实测——Sponza 无 mask 材质可触发；首次引入 mask 材质时必须实跑验证裁剪正确性。
- [ ] Material / Shader | `PrepareAssetsSystem.cpp` 内联 fs 用 `pow(2.2)`/`pow(1/2.2)` 近似 gamma（阶段 5a D8），纹理仍按 `RGBA8_UNORM` 采样、无 SRGB framebuffer，并非正确的色彩管理；引入 SRGB 纹理格式/帧缓冲或完整线性 workflow 后移除两处 pow 近似。
- [x] Material / Shader | `MaterialAsset` 的 normal/MR 贴图已随阶段 4c 经 `SceneLoader` 回填 Handle 落位到 `Assets<TextureAsset>`（D4 只加载不采样），shader 无消费端；阶段 5b 需在 Prepare 侧绑定并由 shader 采样（含 MR 的 G=roughness、B=metallic 解包约定）。5a 期间的临时收窄：`prepareMaterial` 对有 MR 贴图的材质忽略 `.emat` 的 `metallic_factor`/`roughness_factor`（glTF factor 是贴图乘数、有贴图时导出方留 spec 默认 1.0，当常量用会把材质判成全金属杀掉漫反射——2026-08-06 天花板纯色事故的根因），改用占位 metallic 0.0/roughness 0.9；5b 采样 MR 贴图后必须恢复 factor×texture 语义。
  - 完成：2026-08-06（阶段 5b），`prepareMaterial` 扩展 prepare normal/MR 贴图（复用 baseColor 的 pending/fallback/热替换机制），vs 输出 TBN（Gram-Schmidt 正交化，B = cross(N,T)×tangentW），fs 采样法线贴图扰动 N、MR 贴图与 factor 相乘；5a 的 metallic 0.0/roughness 0.9 占位已移除，恢复 factor×texture 语义。

## AI / Agent 基建
> 2026-04-13 讨论纪要：AI 不应是独立的第三个 exe，而是「协议 + 桥梁」。先记录为技术债务，后续渐进补齐。

- [ ] AI / Agent 基建 | `AgentBridge` 直接暴露内部 API，外部 AI 客户端（Claude Desktop / Kimi CLI / Cursor）无法直接接入，需增加 `McpAdapter`，将 `ToolRegistry` 翻译成 MCP `ListToolsResult` / `CallToolRequest`，使引擎成为标准 MCP Server。
  - 参考：`Notes/Agent/引擎-AI-集成实战-MVP.md` 修正 1。
- [ ] AI / Agent 基建 | `AgentBridge::set_component` 直接修改世界状态，若 AI 在 System tick 中途写入会破坏迭代器稳定性，需引入 `CommandBuffer`，`AgentBridge::set_component` 先 stage，在 `Scheduler::tick()` 结束后统一 `apply()`。
  - 参考：路线图。
- [ ] AI / Agent 基建 | AI 在 Runtime 调参或执行「奇怪行为」时无法追踪影响、快速撤销，需在 `World` 内建 `ChangeLog` 记录 `(entity, component, field, old, new)`，实现 `WorldSnapshot::capture / restore`，在 `AgentBridge` 暴露 `snapshot(name)` / `rollback(name)` 工具。
  - 参考：路线图。
- [ ] AI / Agent 基建 | 只读与写工具混在一起，没有统一的安全 gate，需在 `AgentBridge::execute_tool()` 入口处嵌入三级权限（Deny / Ask / Allow），配合组件白名单和 `ApprovalRuntime`，高危操作（如批量删除）强制人工确认，常规写操作自动记录 Undo。
  - 参考：路线图、`Notes/Agent/Permission-System-架构解析`。
- [ ] AI / Agent 基建 | `AgentBridge` 为批处理式返回，Editor 无法同步 AI 的「正在思考」「调用了工具 X」等中间状态，需借鉴 Kimi CLI 的 Wire 协议，定义 `EngineDisplayBlock` 事件流（`diff / changelog / task_status`），`AgentBridge` 在工具执行中途 `emitEvent`，Editor AI Panel 消费并渲染。
  - 参考：`Notes/Agent/Agent-Loop-架构解析`、`Notes/Agent/UI-System-架构解析`。
- [ ] AI / Agent 基建 | 未来复杂任务（如「搭建一个射击关卡」）需要 Director Agent 拆分子任务给 LevelDesignAgent、GameplayAgent 并行执行，需实现 `TransactionalWorld`（主世界 + 沙箱副本）+ `AgentOrchestrator` 冲突检测与合并，每个 Agent 绑定独立 `ApprovalSource` 和组件锁。
  - 参考：路线图、`Notes/Agent/Multi-Agent-架构解析`。
- [ ] Bridge / AI 桥接 | `bridge/private/agent_bridge.cpp` + `bridge/private/tool_registry.cpp` JSON 解析使用手写字符串查找（非完整 JSON parser），仅支持简单参数提取，复杂嵌套结构解析会失败，需引入轻量级 JSON 库或手写递归下降解析器。

## Log / 日志系统
> 2026-05-25：基础功能已完成（设备抽象、JSONL、文件滚动、Once 宏、ImGui 面板）。
> 架构原则：Logger 是底层基础设施，保持全局单例；ECS 通过桥接方式消费日志，而非反向依赖。

- [ ] Log / 日志系统 | `logger.cpp` 双缓冲队列在 >1000 条/帧时存在锁竞争，需引入 TLS 无锁写缓冲（UE TraceLog 模式），每个线程 `thread_local` 预分配 64KB 环形缓冲，三指针模型（Cursor/Committed/Reaped），后台 Worker 定期批量收集，消除锁竞争。
- [ ] Log / 日志系统 | `log_entry.h` 当前日志为纯文本消息，AI 无法按字段过滤（如 `fps < 30`），需引入结构化字段日志（Bevy tracing 风格），`LOG_STRUCT("Render", "frame_stats", field("fps", fps), field("draw_calls", dc))`，JSON 输出增加 `fields` 对象。
- [ ] Log / 日志系统 | `logger.cpp` 掉帧排查时日志与帧时间、实体数、内存用量未关联，需每帧自动注入 `DiagnosticSnapshot`（fps / frame_time_ms / entity_count / memory_used_mb），`LOG_STRUCT("Diagnostics", "frame_snapshot", ...)`。
- [ ] Log / 日志系统 | `logger.h` ECS 侧需桥接日志系统：`LogEvent` 瞬时 ECS Event + `LogSinkSystem` 每帧读取 EventBuffer 并调用 `LOG_INFO` / `LOG_ERROR` 输出到现有 Logger；Logger 本身保持独立单例，不依赖 ECS World。
- [ ] Log / 日志系统 | `log/private/output/file_output.cpp` + `log/private/output/json_file_output.cpp` 各自独立实现滚动逻辑，未来应提取公共基类或统一策略。

## Base Layer / 基础层优化
> 本章节完整融合 `plans/BaseLayer-Optimizations-Plan.md` 全部内容（已完成 + 未做 + 回探替换确认）。原 plan 文件可视为已归档。

### 回探替换时机确认（阶段 1/2 → 阶段 3）

| 回探项 | 当前状态 | 备注 |
|--------|----------|------|
| `std::vector` → `DynamicArray`/`SparseSet` | ✅ 已完成 | ECS 核心已用自研容器 |
| `std::string` → `SmallString`/`StringId` | ✅ 已完成 | 日志、路径、组件名已替换 |
| 裸 `assert()` → `CHECK`/`VERIFY`/`ENSURE` | ✅ 已完成 | `foundation_types.h` 已提供 |
| 裸 `fopen` → VFS | ✅ 已完成 | `vfs/` 模块已存在 |
| 裸 `std::thread` → `ThreadPool` | ✅ 已完成 | `thread_pool/` 已存在 |

### 未做：不影响初版引擎（后续阶段补齐）

- [ ] Base Layer | 线程池 TLS 本地队列快速路径。当前 round-robin 原子计数器不是瓶颈，但阶段 4 System 并行化前必须实施。
  - 参考：知识库 `Notes/SelfGameEngine/基础工具层/线程池与任务系统.md` — `thread_local Worker* s_localWorker` + `SubmitLocal()`
- [ ] Base Layer | 线程池 `waitForAll()` 帮助执行。当前主线程 waitForAll 空转不致命，但与 TLS 本地队列一起改。
  - 参考：知识库 `Notes/SelfGameEngine/基础工具层/线程池与任务系统.md` — 等待线程主动 pop/steal 并执行任务
- [ ] Base Layer | 主线程回调队列 (`MainThreadQueue`)。初版无异步加载/热重载需求，阶段 5 异步加载管线开始前必须完成。
  - 参考：知识库 `Notes/SelfGameEngine/基础工具层/线程池与任务系统.md` — 后台线程通过回调队列投递，主线程 swap 后无锁消费
- [ ] Base Layer | 线程池命名线程 (`Named Threads`)。初版单线程渲染 + 单线程 ECS 足够，阶段 5 渲染管线（RenderThread 引入）前实施。
  - 参考：知识库 `Notes/SelfGameEngine/基础工具层/线程池与任务系统.md` — `AnyThread` / `GameThread` / `RenderThread` / `IOThread`
- [ ] Base Layer | 任务依赖图 DAG + Retraction。初版无复杂异步链式任务，阶段 5 异步加载管线（资源依赖图）前实施。
  - 参考：知识库 `Notes/SelfGameEngine/基础工具层/线程池与任务系统.md` — `TaskNode` + 原子依赖计数器 + `TryRetractAndExecute`
- [ ] Base Layer | `StringInternPool` ECS Resource 化。当前单 World 场景全局单例无问题，且消费者（序列化器、Plugin 管理器、编辑器面板）均非 ECS System，不需要调度器感知读写依赖。触发条件：ECS Resource 基础设施就绪 + 多 World 隔离或编辑器需要独立 Intern 池时再评估。
  - 参考：2026-08-04 讨论结论，知识库 `Notes/SelfGameEngine/基础工具层/字符串系统.md`、`Notes/Bevy/第一阶段-构建与ECS核心/Bevy-bevy_ecs-源码解析：Resource 全局状态.md`
- [ ] Base Layer | `StringId` 增加 intern 池索引。当前字符串碰撞概率极低，与 StringInternPool Resource 化一起改。
  - 参考：知识库 `Notes/SelfGameEngine/基础工具层/字符串系统.md` — `u64 m_hash` + `u32 m_index` 二次校验
- [ ] Base Layer | 日志 `flush()` devices 锁竞争优化。初版日志量小，锁竞争不显著，高并发日志场景前实施。
  - 参考：知识库 `Notes/SelfGameEngine/Hello-Engine-Window/可视化日志系统.md` — 未来替换为 TLS 无锁环形缓冲

## Module / 模块架构
> 来自已完成的模块重构计划，以下拆分/扩展时机未到，但方向已明确。

- [ ] Module / 模块架构 | `core/CMakeLists.txt` 为维持现有裸引 include 风格（如 `"frame_arena.h"`、`"vec.h"`），暴露了 `core/`、`core/memory/`、`core/math/`、`_engine/source/` 四个 include 路径，include 路径过于宽泛，弱化了模块边界。未来应逐步统一为完整路径风格（如 `"core/memory/frame_arena.h"`、`"core/math/vec.h"`），然后收紧 include 暴露。
- [ ] Module / 模块架构 | `math/aabb.h` 曾包含 `#include "type_registry.h"` + `REFLECT_COMPONENT(AABB)`，`math_lib.h` 曾包含 `#include "transform_component.h"`，迫使纯数学库反向依赖 ECS。重构时已移除，但说明历史代码存在模块边界污染，未来需加强 Code Review 防止类似问题。
  - 文件：`core/math/aabb.h`（已修复）、`core/math/math_lib.h`（已修复）。

- [ ] Module / 模块架构 | 当前 `window/` 只含窗口和输入，未来需加入线程池抽象、文件 IO 底层、网络 Socket、CPU/SIMD 硬件检测、动态库加载，需扩展为 `platform/` 下设 `window/`、`input/`、`thread/`、`filesystem/`、`network/` 子目录。
- [ ] Module / 模块架构 | 反射系统（atom_registry、type_registry、inspector_reflection）与资源管理（prefab、scene_serializer）已独立为多个文件，文件数 > 5，值得独立，需从 `core/` 拆分出 `reflect/` 和 `asset/`，`reflect/` 负责类型注册/属性遍历/序列化/Inspector 自动生成，`asset/` 负责 Handle/异步加载/引用计数/热重载。
- [ ] Module / 模块架构 | `core/` 与具体业务之间缺少"比 core 更业务、比 gameplay 更通用"的层，需引入 `common/` 通用中间件层，包含场景图（Transform 层级脏标记传播）、Prefab 资产结构、序列化框架、状态机基础、调试绘制接口。
- [ ] Module / 模块架构 | `render/CMakeLists.txt` 直接 `PUBLIC_DEPS EcsLib`，导致整个渲染模块与 ECS 框架强耦合。当前 `RenderLib` 内部混合了两层职责：底层渲染能力（RHI、GPU 资源、RenderPhase、SortKey）和上层 ECS 驱动的渲染管线（`RenderWorld`、`ExtractSchedule`、各类 extract/cull/queue systems）。未来应拆分为 `RenderCoreLib`（零 ECS 依赖，可被 headless 工具/服务端/测试复用）和 `RenderSystemLib`（依赖 `RenderCoreLib` + `EcsLib`，容纳 ECS 组件与 systems）。短期可先记录，待 RHI 后端稳定后再动手拆分。
  - 涉及文件：`_engine/source/render/CMakeLists.txt`、`render/public/render_world/*`、`render/public/extract/*`、`render/public/culling/*`、`render/public/queue/*`、`render/public/components/*`、`render/private/**/*`。

- [x] Module / 模块架构 | `imgui/CMakeLists.txt` 直接 `PUBLIC_DEPS EcsLib`，仅因为 `imgui_panels.cpp` 提供了 `buildECSInspector(World&, Scheduler&, ...)` 这一调试面板。ImGui 作为 UI 框架封装层不应依赖 ECS；ECS Inspector 属于 Editor/调试工具层，应迁到独立的 `EditorLib` 或 `_game/source/editor_debug` 模块，只保留 `ImGuiManager`、`initImGui()`、`buildDockSpace()`、`buildDebugPanel()`、`buildLogPanel()` 在 `ImGuiLib` 中。
  - 完成：2026-08-04，创建 `_engine/source/editor/` (EditorLib)，将 `buildECSInspector` + `drawField` 迁至 `editor_panels.cpp`，将 `AtomRegistry::registerBuiltinAtoms()` 实现迁至 `editor_atom_registry.cpp`。ImGuiLib 移除 `EcsLib` PUBLIC_DEPS，零 ECS 依赖。
  - 涉及文件：`_engine/source/imgui/CMakeLists.txt`、`imgui/public/imgui_panels.h`、`imgui/private/imgui_panels.cpp`。

- [ ] Module / 模块架构 | `BridgeLib` 命名过于模糊，当前依赖 `EcsLib` + `MotorLib` + `LogLib`，职责是「AI 与引擎的桥接 + 运行时装配」。未来应明确其边界：若是 AI 协议适配器，应改名为 `AgentBridgeLib` 并只负责协议翻译；若是 ECS 与 Motor 的胶水层，应独立为 `MotorEcsAdapterLib`；若是运行时插件/系统注册，应上提到 `RuntimeLib` 或 `App` 层。
  - 涉及文件：`_engine/source/bridge/CMakeLists.txt`、`bridge/private/agent_bridge.cpp`、`bridge/private/tool_registry.cpp`。

- [ ] Module / 模块架构 | 当前 VS 解决方案已通过 `FOLDER` 属性按源码树分组（`Engine\core`、`Engine\ecs`、…、`Game\runtime`、`Launcher`、`Tests`），新增模块会自动按路径进入对应文件夹。未来若引入 `Plugins/` 或 `Tools/` 顶层目录，需扩展 `cmake/EntelechyModule.cmake` 中的 folder 映射规则。
- [x] Module / 模块架构 | `math/aabb.h:42` 注册了 ECS 组件 `REFLECT_COMPONENT(AABB)`，迫使 `math` 模块依赖 `core/type_registry.h`，破坏底层纯净性，需在 `render/components/` 下新建 `WorldAabb.h`（主世界）与 `RenderAabb.h`（渲染世界）作为专用 ECS 组件，`math/aabb.h` 移除 `type_registry.h` 依赖，恢复零依赖。
  - 完成：2026-08-05。`type_registry.h` 依赖此前已移除（见上方已修复条目）；本次补齐专用组件：`render_system/public/components/WorldAabb.h`（`WorldAABB`）+ `RenderComponents.h` 内 `RenderAABB`。`RenderAABB` 直接并入 `RenderComponents.h` 而非独立 `RenderAabb.h`，与 `RenderMesh`/`RenderTransform` 同处。

## Core Runtime / 阶段 4 差距（尚未实施）
> 以下项来自 SelfGameEngine 第四阶段知识库的「默认推荐」路径，当前代码已实现骨架但关键特性缺失。

- [ ] Core Runtime | **双轨时间步（Main + FixedMain）**。当前 `Scheduler::tick()` 只有单一 Fixed 轨道（60Hz），所有 System 都跑固定步长。需拆分 `Main`（可变步长，渲染/UI/相机跟随）与 `FixedMain`（固定步长，物理/AI/确定性逻辑），`RunFixedMainLoop` 作为桥接，渲染插值消除卡顿。
  - 参考：知识库 `Notes/SelfGameEngine/核心运行时闭环/系统调度与确定性.md` 问题 5
- [ ] Core Runtime | **System 级并行调度**。当前 `Scheduler::tickFixed()` 是纯串行执行。需构建期分析 `reads/writes` 冲突图，按 Wavefront 分组，同波次无冲突 System 并行执行。
  - 参考：知识库 `Notes/SelfGameEngine/核心运行时闭环/系统调度与确定性.md` 问题 4
- [ ] Core Runtime | **事件总线 Pub-Sub**。当前事件只是普通 ECS 组件（`KeyboardEvent`、`ColorChangeEvent`、`DeathEvent`），无通用事件总线。需设计 `EventBus` / `EventReader<T>` / `EventWriter<T>`，消费后自动清理，支持延迟投递。
  - 参考：知识库 `Notes/SelfGameEngine/核心运行时闭环/事件总线.md`
- [ ] Core Runtime | **命令缓冲扁平化**。当前 `CommandBuffer` 每条命令单独 `DefaultAllocator::alloc` + 虚函数 `ICommand::apply`，大量命令时分配开销大。应改为类型擦除的固定容量 buffer（`DynamicArray<u8>` 存扁平命令 + 函数指针表），并引入线程本地命令队列。
  - 参考：知识库 `Notes/SelfGameEngine/核心运行时闭环/系统调度与确定性.md` 问题 3
- [ ] Core Runtime | **快照/回放/确定性**。缺少 `WorldSnapshot` 捕获与恢复、增量 Diff、确定性保证清单（稳定拓扑排序 FIFO、确定性 RNG、禁止未初始化内存）。
  - 参考：知识库 `Notes/SelfGameEngine/核心运行时闭环/系统调度与确定性.md` 问题 6
- [ ] Core Runtime | `GlobalTransform` 用 `Affine3A` 替代 `Mat4`。`Affine3A` 48 字节 vs `Mat4` 64 字节，省 25% 内存，且能正确表达层级叠加后的 shear。Bevy 生产验证。
  - 参考：知识库 `Notes/SelfGameEngine/核心运行时闭环/场景图与变换.md` 问题 2
- [ ] Core Runtime | `imgui_panels.cpp` 中仍有 `Fallback: legacy ComponentDesc recursive lookup` 分支，新增组件若未走新反射路径会静默回退到旧逻辑，需补全 `AtomRegistry::registerBuiltinAtoms()` 覆盖所有引擎内置原子类型，`imgui_panels.cpp` 中删除 legacy 分支，强制走 `inspectorDrawComponent()` 递归绘制。
- [ ] Core Runtime | `ViewBinnedPhases` / `ViewSortedPhases` / `ViewVisibleList`（`render/RenderResources.h`、`render/culling/ViewVisibleList.h`）未注册 `REFLECT_COMPONENT`，Inspector 和序列化系统无法遍历字段，需补全注册，并确认 `ViewVisibleList` 中的 `DynamicArray<Entity>` 反射系统是否支持容器字段（当前可能只支持 Atom/Composite）。
- [ ] Core Runtime | 反射系统 `REG_FIELD` 宏基于 `offsetof` + 类型名字符串化，不支持嵌套/复合类型字段。阶段 2a 将 `MeshAssetRef`/`MaterialAssetRef`/`RenderMesh`/`RenderMaterial` 的 `u32 asset_id` 迁移为 `Handle<T>`（8 字节 index+generation）后，Handle 字段无法注册反射——`MeshAssetRef`/`MaterialAssetRef`/`RenderMesh` 目前注册为无字段组件，`RenderMaterial` 仅保留 `render_phase` 字段（`render_system/private/components/component_registration.cpp` 有 NOTE 注释）。需反射系统支持复合类型（如注册 `asset_id.index`/`asset_id.generation` 子字段或自定义 field descriptor）后补回。

## Build System / 构建体系重构
> 2026-05-30：完成 Phase 1 — 引入 `entelechy_module()` 宏、去掉代理层、统一模块声明、CMake 直接驱动模块发现。
> 2026-05-30：完成 Phase 2 — 清理模块边界，去掉 `..` PUBLIC 暴露，统一跨模块 include 为裸文件名风格，修复隐式依赖。

- [ ] Build System | `test_runner/CMakeLists.txt` 使用 `$<TARGET_OBJECTS:*Tests>` 收集测试对象文件，该 generator expression 在部分 CMake 生成器或平台上行为不一致，若未来切换到 Ninja/Make 需验证兼容性。
  - 文件：`_engine/source/test_runner/CMakeLists.txt`
- [ ] Build System | `launch/generator.py` 已标记弃用但尚未移除，作为 standalone `main.cpp` 生成的 fallback 保留。待团队完全切换到纯 CMake 流程后删除。
  - 文件：`launch/generator.py`
- [ ] Build System | `main.cpp.in` 模板中硬编码了大量跨模块 include（如 `#include "glfw_window.h"`、`#include "render/opengl_backend.h"`），未通过模块依赖自动推导。未来应让各模块在 `entelechy_module()` 中声明「需要暴露给 main 的头文件」，或由 CMake 自动生成 include 列表。
  - 文件：`launch/templates/main.cpp.in`
- [x] Build System | 保留 `Visual Studio` generator 的同时新增 `build_ninja/` 用于生成 `compile_commands.json`。已通过项目本地 `.venv` 固定 `conan==2.30.0`，并在 Windows 下跑两次 `conan install`（Debug/Release）补齐 VS Multi-Config 所需依赖信息，构建已跑通。
  - 文件：`scripts/build/build.py`、`requirements.txt`、`scripts/tools/setup_env.py`
- [ ] Build System | CMake 配置阶段仍会打印 `IMPORTED_LOCATION not set for imported target "CONAN_LIB::xxx_DEBUG" configuration "Release"` 等错误（CMakeDeps 对兼容包的 Multi-config target 处理不完整），虽不影响编译与 `compile_commands.json` 生成，但污染输出。已新增 `--strict-build` 参数可作为消除该噪音的入口，但当前环境因 `bmlib`  remote 中的定制 `cmake/3.25.3.5` 指向私有 GitLab，启用 `--strict-build` 会触发无权限错误，因此 task 中未默认启用。
  - 文件：`scripts/build/build.py`、`.zed/tasks.json`、`.vscode/tasks.json`、`AGENTS-BUILD.md`
- [ ] Build System | Conan install 阶段仍会打印 `env_info`、`cpp_info.filenames/names/build_modules` 等 Conan 1.X 特性 deprecated warnings，来自 conan-center 上游包，暂无法在本项目消除。待 conan-center 包更新或 Conan 弃用这些 warning 后自然消失。
  - 文件：`conanfile.py`（依赖上游）

## ThreadPool / 线程池

- [ ] ThreadPool / 线程池 | `thread_pool/public/thread_pool/thread_pool.h` `WorkStealingQueue` 固定容量 4096，本地队列满时回退到全局 `overflowMutex` + `std::deque`，极端负载下可能阻塞，需评估是否引入动态扩容或更优溢出策略。

## VFS / 虚拟文件系统

- [ ] VFS / 虚拟文件系统 | `vfs/private/mount_point.cpp` `FileSystemMountPoint` 使用 `fopen/fread/fwrite` 做文件 IO，路径拼接限制 512 字节缓冲区，未来应支持超长路径和异步 IO。
- [ ] VFS / 虚拟文件系统 | `VFS::mount()` 接口签名表现为非拥有（裸 `IMountPoint*`），但 `VFS::clear()` 实际会对 backend 调 `destroy_at` + `DefaultAllocator::free`——即 VFS 隐式取得所有权且要求 backend 必须用 `DefaultAllocator::alloc` 分配（成员对象或 `new` 分配都会在卸载时 AV，2026-08-05 阶段 2c 关闭崩溃根因之一）。所有权约定仅由实现暗示，接口无任何注释/类型约束，应在接口层显式化（注释 + 文档，或改为 `UniquePtr`/非拥有语义）。

## Window / 窗口系统

- [ ] Window / 窗口系统 | `window/public/window/window/window.h` `IWindow` 目前只有 GLFW 实现，未来需加入 SDL / Win32 后端。
- [ ] Window / 窗口系统 | `window/public/window/window/glfw_window.h` `getNativeDisplay()` 是 Vulkan 创建 Surface 的预留 stub，未实现。
- [ ] Window / 窗口系统 | `FlyCameraSystem`（`_game/source/runtime/private/fly_camera_system.cpp`）用右键拖拽做视角控制，因 `GlfwWindow` 未封装 `glfwSetInputMode(GLFW_CURSOR, GLFW_CURSOR_DISABLED)` 无鼠标捕获，拖拽到窗口边缘会中断，需窗口层补捕获/隐藏光标 API 后改为 FPS 式捕获视角。

## Runtime / 游戏运行时

- [ ] Runtime / 游戏运行时 | `_game/source/runtime/private/game_runtime.cpp` `main.cpp` 由 `launch/generator.py` 构建时生成，主循环逻辑散落在模板中，未来 Runtime 应接管更多主循环逻辑。
- [ ] Runtime / 游戏运行时 | `_game/source/runtime/public/render_assets.h` 的 `renderAssets()` 使用函数内 static 全局存储持有 VFS + `AssetServer` + 三类 `Assets<T>` 与缓存 Handle（2c 后规模进一步增大），根因是 ECS 无 Resource 概念（见 ECS 条目）。待 ECS Resource 基础设施就绪后，资产存储应注册为 World 级全局数据，此静态层整体移除。
- [x] Runtime / 游戏运行时 | `_game/source/runtime/private/scene_loader.cpp` 的 `ManifestCursor` 是工程内第三个手写 JSON 片段解析器（`ecs/private/prefab/scene_serializer.cpp` 的 static `JsonCursor`、`bridge` 的字符串查找之后），仅支持 `scene.json` 固定 schema；清单格式若继续演化（嵌套/可选字段），应抽一个公共极简 JSON 解析器到 core 供复用。
  - 完成：2026-08-05，提取 `core/public/json/json_cursor.h`（`JsonCursor`，两份私有实现的并集），`scene_serializer.cpp` 与 `scene_loader.cpp` 均已迁移；新增 `core/tests/test_json_cursor.cpp` 9 个用例。`bridge` 的字符串查找解析仍是独立问题（见 Bridge 条目）。
- [x] Runtime / 游戏运行时 | 引擎无帧读回/截图机制，渲染验收只能靠日志佐证（阶段 3c 的 Sponza 几何完整性即未截图确认）；需在 RHI/窗口层补 readback + 截图键（或离屏 capture 工具）。
  - 完成：2026-08-07，`IRHIDevice::readbackBackbuffer()`（GL 实现：glReadPixels 后台缓冲 + 垂直翻转，须在 present 前调用）+ `render/screenshot/saveScreenshotPng()`（stb_image_write）。触发方式：F9 热键写 `logs/screenshots/screenshot_<ts>.png`；或环境变量 `ENTELECHY_SCREENSHOT_FRAME=<n>`（第 n 帧自动截图）、`ENTELECHY_SCREENSHOT_PATH`、`ENTELECHY_EXIT_AFTER_SCREENSHOT=1`（截图后退出），便于自动化验收。
- [x] Runtime / 游戏运行时 | `_game/source/runtime/private/scene_loader.cpp` 的 `spawnCookedScene` 是引擎工具 `mesh_cooker` 产物（引擎自有格式）的唯一消费者，格式归引擎、解析归游戏层不对称，第二个游戏需原样重写；阶段 4 补材质引用时随场景加载入口一并迁入引擎（`asset/` 或独立 scene 模块），游戏侧只传场景路径（2026-08-05 分层审视结论）。
  - 完成：2026-08-06（阶段 4c，D5），迁入引擎新模块 `_engine/source/asset/scene/`（SceneLib）：`SceneLoader` 构造注入 VFS/`AssetServer`/loader/`Assets<T>`（引擎不持有游戏侧全局），自持 `MaterialAssetLoader` 与场景材质清单；游戏侧 `GamePlugin::setup()` 场景加载剩一行 `renderAssets().scene_loader.spawnCookedScene(world, "sponza/cooked/scene.json")`，`RenderAssets` 的 `material_loader`/`scene_materials` 移除。
- [ ] Runtime / 游戏运行时 | `_game/source/runtime/public/render_assets.h` 中 VFS 双挂载（含 cwd 兼容）、`AssetServer` + 标准 loader 注册、三类 `Assets<T>` 存储与每帧 `processEvents()`（`launch/templates/main.cpp.in`）是任何游戏都要原样搭建的资产子系统装配；与 RENDER_LAYER_PROGRESS 5.8/5.9 资产系统升级重叠，届时由引擎提供资产子系统引导（挂载约定、loader 注册、事件泵），游戏层只声明内容目录与资产（2026-08-05 分层审视结论）。

## Core / 基础库扩展缺口

- [x] Core / 基础库扩展缺口 | `HashMap::clear()`（`core/container/hash_map.h`）曾将 key/value 就地析构后不重建，而 `~HashMap`/`grow`/移动赋值按「所有槽位始终存活」不变量对每个槽位再析构一次——clear 时存在的每个值都会被二次析构（POD 无害，RAII 类型如 `RHIRef` 会对已释放的 GPUResource 二次 release，关闭窗口时 AV 崩溃，2026-08-05 由阶段 2c Prepare 材质表触发）。
  - 完成：2026-08-05，`clear()` 析构后就地重构造默认 key/value，维持槽位存活不变量；新增 `core/tests/test_containers.cpp`（RAII 探针断言构造/析构精确平衡 + clear 后槽位可复用）。

> 2026-05-31：当前代码已按「基础库优先」规则消除了所有可替换的 STL/裸分配依赖。以下设施因基础库尚未提供对应实现，暂时保留 STL，待扩展后统一迁移。

- [ ] Core / 基础库扩展缺口 | `std::unique_ptr<T>` 仍在 `log/core/logger.h`（`addOutputDevice` / `m_devices`）与 `render/example/simple_cube_renderer.h`（`m_device`、`m_shader_cache`）中使用。需引入引擎级 `UniquePtr<T, Deleter>`（支持自定义 `DefaultAllocator` 释放），并迁移所有所有权语义场景。
- [ ] Core / 基础库扩展缺口 | `std::function<void()>` 仍在 `thread_pool/thread_pool.h`、`asset/loader/asset_server.h`、`bridge/tool_registry.h` 中使用。需设计 SBO（小对象优化）的 `UniqueFunction<Ret(Args...)>` 或 `Delegate`（参考 `ue::TFunction`），保证 move 到线程池队列时不通过标准库默认分配器分配。注意：`FunctionRef`（非拥有型引用，类似 C++23 `std::function_ref`）不适用于需要存储任务的线程池场景，因其不拥有被引用对象。<br/>2026-08-04 评估：原 roadmap 中的 `FunctionRef` 方案定位错误，正确方案是 SBO `UniqueFunction`，建议先 profile 确认 `std::function` 是否在热路径上触发堆分配后再实施。
- [ ] Core / 基础库扩展缺口 | `std::deque<T>` 仍在 `thread_pool` 溢出队列与 `asset_server` 任务队列中使用。需实现支持头尾 O(1) push/pop 的 `Deque<T>`，或更直接地提供 lock-free `MPSCQueue<T>` 替换线程池/资源加载的回调队列。
- [x] Core / 基础库扩展缺口 | `std::sort` 仍在 `render/queue/SortedRenderPhase.cpp` 中使用。需引入 `algo::sort(begin, end, cmp)`（可考虑 introsort / timsort），并对 `DynamicArray` 提供 convenience 成员方法。
  - 完成：2026-06-18，新增 `core/algorithm/radix_sort.h` 提供稳定 64-bit LSD radix sort，`SortedRenderPhase::prepare()` 改用 `radixSort64` 按 `SortKey` 排序；新增 `RadixSort64` 单元测试。后续若 Sort Key 结构变化或需通用比较排序，再引入 `algo::sort`。
- [ ] Core / 基础库扩展缺口 | `std::thread` / `std::mutex` / `std::atomic` / `std::condition_variable` 仍散落在线程池、asset_server、logger 中。需封装为 `Thread`、`Mutex`、`Atomic<T>`、`ConditionVariable` 等薄层，便于未来切换平台线程模型（如 Windows ThreadPool API、C++20 `std::jthread`）。

> 2026-08-04 删除 `plan/ENTELECHY_ROADMAP.md`。其中两项低优先级事项评估结论：
> - **`entelechy_snprintf` 跨平台封装**（原 roadmap #3）：不需要。MSVC 2015+ `snprintf` 已是 C99 兼容，与 POSIX 行为一致，`string_format.h` 中所有调用均传入已知 buffer 大小 + 字面量格式串，无实际风险。已归档。
> - **`FunctionRef` 替代 `std::function`**（原 roadmap #4）：方案定位错误。`FunctionRef` 是非拥有型引用，不适用于需要存储任务的线程池队列。正确方向是 SBO `UniqueFunction`，但应先 profile 确认热路径堆分配后再实施。已合并到上方 `std::function<void()>` 条目中。

## Allocator / ECS 存储优化（2026-05-31 完成阶段一至四基础实现，以下待后续细化）

- [ ] Allocator / ECS 存储优化 | `ecs/public/ecs/archetype/archetype_world.h` `ArchetypeWorld` 目前与 `World`（SparseSet+Column）并存，未实现双轨迁移路径。`ArchetypeWorld` 缺少 `setParent`、批量 `spawn`、事件系统、`CommandBuffer` 等能力，无法直接替换现有 `World`。需在 `App` / `Scheduler` 中支持可选的 world 后端，逐步将 System 从 `World` 迁移到 `ArchetypeWorld`。
  - **阻塞点**：`System::tick(World&, ...)` / `Scheduler::tick(World&, ...)` / `CommandBuffer::apply(World&)` 签名硬编码旧 `World`；`Query<Cs...>` 模板依赖 `World::getComponentArray`；`AgentBridge` 直接遍历 `world.componentArrays()`；渲染系统的 `IExtractSystem` 接口也硬编码 `World&`。迁移需跨 `ecs` / `motor` / `render` / `bridge` / `_game` 多个模块，非 ECS 模块内部可独立完成。
- [ ] Allocator / ECS 存储优化 | `test_runner/CMakeLists.txt` 依赖 `entelechy_get_enabled_modules` 收集测试 OBJECT 库，但各模块 `tests/CMakeLists.txt` 使用裸 `add_library(... OBJECT)` 而非 `entelechy_module()`，导致测试目标无法被自动发现。当前 workaround 是在每个 `tests/CMakeLists.txt` 末尾手动 `set_property(GLOBAL APPEND PROPERTY ENTELECHY_ENABLED_MODULES ...)`，应统一改用 `entelechy_module(NAME XxxTests TYPE OBJECT NO_TESTS)`。
