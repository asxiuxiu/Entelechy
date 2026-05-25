# SelfGameEngine TODO / 技术债务

## ECS / Core Runtime
- [ ] ECS / Core Runtime | `src/world.h` 中 `std::vector<bool> alive` 存在位压缩特化性能陷阱，读写较慢且无法返回真实引用，需替换为 `std::vector<uint8_t>` 或引入 Sparse Set 架构。
- [ ] ECS / Core Runtime | `src/world.h` 中 `positions` / `velocities` 数组只增不减，先 spawn 大量实体再销毁后数组仍保持峰值大小，浪费内存，需升级为 Sparse Set（`dense + sparse`）让存活数据保持紧凑。
- [ ] ECS / Core Runtime | `AgentBridge`（`_engine/source/bridge/agent_bridge.cpp:12`）越界承担 System 注册职责，直接持有 `World`、`Scheduler`、`MovementSystem` 并在 `init()` 中注册，把「AI 接口桥梁」与「运行时装配」耦合在一起，需将装配逻辑上提到 Runtime / App 层，`AgentBridge` 仅通过指针/引用访问外部注入的世界与调度器。

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
- [ ] Asset / 资源管理 | `HandleTableSlot<T>` 使用 `DynamicArray`，resize 时默认构造元素，要求 T 必须可默认构造，需重构为手动内存管理（`alignas(T) char buffer[sizeof(T)]` + placement new），类似 `std::optional` 或 `Column<T>` 的底层实现。
- [ ] Asset / 资源管理 | AI Agent 需要自描述地了解「当前加载了哪些资源、各占多少内存、引用关系如何」，需通过反射系统注册资源类型 Schema，`AssetServer` 暴露 `query_asset(handle)` / `dump_ref_graph()` 等 MCP 工具。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/资源管理.md` 问题 8。

## Render / RHI
- [ ] Render / RHI | `GLRHIDevice` 未包装为 ECS `Resource`，`MeshRenderSystem` 仍直接调用 GL 裸接口，需让渲染系统通过 `IRHICommandList` 录制命令。
  - 参考：`_engine/source/render/rhi_device.h`
- [ ] Render / RHI | 当 Draw Call > 2000 且 Profiling 确认 CPU 瓶颈时，`GLCommandList` 的即时执行模式无法扩展，需引入简化版延迟命令缓冲（`LinearAllocator` + 引擎级命令枚举 + `switch-case` 翻译执行），单线程录制 → 多线程并行生成 `DrawPacket` → 单线程排序 → 单线程翻译。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 2/3。
- [ ] Render / RHI | CPU 删除纹理但 GPU 还在读的 race condition，需引入 RHI 内部延迟删除队列，引用计数归零后进入队列，帧边界检查 GPU Fence，确认 GPU 完成后批量释放。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 4。
- [ ] Render / RHI | `PSOManager::getOrCreateAsync()` 缓存未命中时同步编译造成帧时间尖刺（hitch），需返回占位 PSO（如最简单的纯色着色器），同时启动后台线程编译，后台完成后自动切换。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 5。
- [ ] Render / RHI | RHI 抽象接口目前仅有 OpenGL 后端，需在接口已跑通带纹理旋转立方体、接口稳定后启动第二个后端，先钉死 D3D12（Windows 默认，调试工具顶尖），跑通全部上层管线后再移植 Vulkan。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 7。
- [ ] Render / RHI | 主 World 与渲染未分离，无法支持 CPU-GPU 流水线并行、多视图隔离、确定性快照，需实现双 World 模型（主 World 跑逻辑 30Hz，Render World 跑渲染 60Hz），每帧 Extract 阶段单向只读复制可渲染数据到 Render World，Render World 内 System 生成命令，最终 `PresentSystem` 输出。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 6。
- [ ] Render / RHI | `FrustumCullSystem`（`render/culling/FrustumCullSystem.cpp:37`）与 `QueueDrawsSystem`（`render/queue/QueueDrawsSystem.cpp:31`）每帧通过 `Query<ViewVisibleList>` / `Query<ViewBinnedPhases>` / `Query<ViewSortedPhases>` 遍历全部实体来定位唯一的 Resource-like 组件，且不存在时会创建新实体，导致 render world 实体数只增不减，需将 View-level Resource 绑定到 view 实体或引入 ECS 的 Resource 系统（非组件的全局表）。
- [ ] Render / RHI | `BinnedRenderPhase`（`render/queue/BinnedRenderPhase.h`）使用 `DynamicArray<PhaseBin>` 线性分箱，因 ECS `ComponentArray::set` 要求组件可拷贝而 `HashMap` 显式 delete 了拷贝，为绕过限制改用线性查找，大规模场景下退化为 O(n)，需恢复 `HashMap` 或引入独立 Resource 系统。
- [ ] Render / RHI | `QueueDrawsSystem`（`render/queue/QueueDrawsSystem.cpp:65`）深度计算使用物体原点 `worldMatrix * Vec3{0,0,0}` 的 z 值，对于中心偏移的大型物体排序结果不准确，若实体有 `AABB` 应使用 `(aabb.min + aabb.max) * 0.5f` 作为深度采样点，无 AABB 则回退到原点。
- [ ] Render / RHI | `QueueDrawsSystem`（`render/queue/QueueDrawsSystem.cpp:48`）SortKey 的 float→uint 编码未处理 viewZ ≤ 0，IEEE-754 负数 bit pattern 按 uint 排序与数值排序不一致，若物体在相机后方或近平面处深度键会混乱，需将 viewZ 钳制到 `[near, far]` 后规范化到 `[0, 1]` 再编码为 uint（如 `uint32_t(depth * 0xFFFFFFFF)`），保证全范围单调。
- [ ] Render / RHI | `FrustumCullSystem` 和 `QueueDrawsSystem` 由调用方手动按顺序调用，无内建依赖声明，未来阶段增多后容易顺序出错，需仿照 `ExtractSchedule` 引入 `RenderSchedule`（`IRenderSystem` 接口 + 注册表），在 `RenderWorld` 中统一定义 Extract → Cull → Queue → ... 的 SystemSet 链。
- [ ] Render / RHI | Culling 与 Queue 系统仅处理第一个 `ExtractedView`，`ViewVisibleList`、`ViewBinnedPhases`、`ViewSortedPhases` 在设计上应每 view 一份，需将 View-level Resource 改为与 view 实体绑定（`Entity viewEntity` 作为 key），Culling 和 Queue 系统遍历所有含 `ExtractedView` 的实体，为每个 view 生成独立的可见列表和 phase 容器。
- [ ] Render / RHI | `PhaseItem::instance_count`（`render/queue/PhaseItem.h`、`render/queue/QueueDrawsSystem.cpp`）字段已预留为 1 但未实现 instancing 合并，`QueueDrawsSystem` 未检测同 material + 同 mesh 的连续实体，需在 `BinnedRenderPhase::addItem` 中检测并合并为同一 `PhaseItem` 且累加 `instance_count`，并配合 Prepare 阶段生成 instance buffer。
- [ ] Render / RHI | `ExtractRenderablesSystem`（`render/extract/ExtractRenderablesSystem.cpp:23`）每帧对静态 AABB 全量拷贝，大多数模型的本地 AABB 是静态的但每帧都通过 `mainWorld.getComponent<AABB>(entity)` 提取到 render world，需在 `MainWorldSync` 中记录「上一帧是否有 AABB」或引入脏标记机制，仅当 AABB 组件被修改时才重新提取。

## Material / Shader
> 2026-05-25：当前实现为同步编译 + CPU uniform 块 + `glUniform*` 即时上传 + 无模板分层。以下为与工业级方案的差额，后续按阶段补齐。

- [ ] Material / Shader | 当前 `Material` 直接硬编码 VS/FS，无变体管理，美术资产与 GPU 状态未解耦，需引入模板-实例三层：`ShaderTemplate` → `MaterialAsset` → `MaterialInstance`，`ShaderTemplate` 定义 `Category/Keyword` 变体规则，`MaterialAsset` 存储参数/PassHint/RenderState，`MaterialInstance` 运行时维护 `TechniqueCache[permutation_id]`。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 1 分支 B。
- [ ] Material / Shader | `Material::init()` 同步编译，加载新材质时阻塞渲染线程，需引入 `TechniqueState` 状态机（Invalid → Pending → Valid），缓存未命中时返回预编译 Fallback（如粉色棋盘格），后台线程编译完成后自动切换。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 4 分支 B。
- [ ] Material / Shader | 每 draw call 遍历参数调用 `glUniform*`，CPU 开销大且无法合批，需定义 `BindGroupLayout` 并按类型分池，`MaterialBindGroupAllocator` 管理，`CPU uniform` 块一次性写入 GPU UBO，`bind()` 只切换 UBO offset 或 PushConstants。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 3 分支 C。
- [ ] Material / Shader | 当前 `Material` 为纯 C++ 对象未接入 ECS，渲染系统无法批量处理同材质实体，需引入 ECS 组件 `MaterialHandle { AssetId material; }`，渲染架构分 Extract → Prepare → Queue → Render，`PrepareMaterialSystem` 按类型批量创建 `BindGroup`，`QueueOpaqueDraws` 按（材质类型, 管线 key）分组排序。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 5 分支 B。
- [ ] Material / Shader | 当前一材质一 PSO，无法表达「有法线/无法线」「有骨骼/无骨骼」等组合变体，需 `ShaderTemplate` 维护 `Array<ShaderCategory>`，`MaterialInstance::getTechnique(keywords)` 生成 `permutation_id` 并查找/触发编译，严格限制维度（≤5 个二值开关）防止组合爆炸。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 2 分支 C。
- [ ] Material / Shader | 当前 Material 只绑定单个 PSO，无 PassHint 概念，需 `MaterialAsset` 增加 `PassHint`（Deferred/Forward/Translucent/UI），渲染系统按 Pass 分批提取对应 Technique，同一材质在不同 Pass 使用不同 shader 组合。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 工业级设计清单。
- [ ] Material / Shader | 当前材质只能通过 C++ 代码定义，美术无法直观调整，需运行时解析节点图定义 → 生成 GLSL/HLSL 源码 → 通过 `ShaderCache` 编译，编辑器通过 MCP/反射接口让 AI 也能操作节点参数。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/材质与着色器系统.md` 问题 1 分支 C。
- [ ] Material / Shader | `MaterialHandle`（`render/components/MaterialHandle.h`）缺少 `render_phase` 信息，`ExtractRenderablesSystem`（`render/extract/ExtractRenderablesSystem.cpp:20`）无法推断 phase，全部默认 `Opaque3D`，透明材质被错误分箱到 `BinnedRenderPhase`，需在材质系统（`Material` / `MaterialAsset`）增加 `RenderPhase` 声明，`ExtractRenderablesSystem` 从材质元数据读取 phase。

## AI / Agent 基建
> 2026-04-13 讨论纪要：AI 不应是独立的第三个 exe，而是「协议 + 桥梁」。先记录为技术债务，后续渐进补齐。

- [ ] AI / Agent 基建 | `AgentBridge` 直接暴露内部 API，外部 AI 客户端（Claude Desktop / Kimi CLI / Cursor）无法直接接入，需增加 `McpAdapter`，将 `ToolRegistry` 翻译成 MCP `ListToolsResult` / `CallToolRequest`，使引擎成为标准 MCP Server。
  - 参考：`Notes/Agent/引擎-AI-集成实战-MVP.md` 修正 1。
- [ ] AI / Agent 基建 | `AgentBridge::set_component` 直接修改世界状态，若 AI 在 System tick 中途写入会破坏迭代器稳定性，需引入 `CommandBuffer`，`AgentBridge::set_component` 先 stage，在 `Scheduler::tick()` 结束后统一 `apply()`。
  - 参考：路线图 `Phase 1.4`。
- [ ] AI / Agent 基建 | AI 在 Runtime 调参或执行「奇怪行为」时无法追踪影响、快速撤销，需在 `World` 内建 `ChangeLog` 记录 `(entity, component, field, old, new)`，实现 `WorldSnapshot::capture / restore`，在 `AgentBridge` 暴露 `snapshot(name)` / `rollback(name)` 工具。
  - 参考：路线图 `Phase 2.1 / 2.2`。
- [ ] AI / Agent 基建 | 只读与写工具混在一起，没有统一的安全 gate，需在 `AgentBridge::execute_tool()` 入口处嵌入三级权限（Deny / Ask / Allow），配合组件白名单和 `ApprovalRuntime`，高危操作（如批量删除）强制人工确认，常规写操作自动记录 Undo。
  - 参考：路线图 `Phase 2.5`、`Notes/Agent/Permission-System-架构解析`。
- [ ] AI / Agent 基建 | `AgentBridge` 为批处理式返回，Editor 无法同步 AI 的「正在思考」「调用了工具 X」等中间状态，需借鉴 Kimi CLI 的 Wire 协议，定义 `EngineDisplayBlock` 事件流（`diff / changelog / task_status`），`AgentBridge` 在工具执行中途 `emitEvent`，Editor AI Panel 消费并渲染。
  - 参考：`Notes/Agent/Agent-Loop-架构解析`、`Notes/Agent/UI-System-架构解析`。
- [ ] AI / Agent 基建 | 未来复杂任务（如「搭建一个射击关卡」）需要 Director Agent 拆分子任务给 LevelDesignAgent、GameplayAgent 并行执行，需实现 `TransactionalWorld`（主世界 + 沙箱副本）+ `AgentOrchestrator` 冲突检测与合并，每个 Agent 绑定独立 `ApprovalSource` 和组件锁。
  - 参考：路线图 `Phase 3.6`、`Notes/Agent/Multi-Agent-架构解析`。

## Log / 日志系统
> 2026-05-25：阶段 0~2 已完成（设备抽象、JSONL、文件滚动、Once 宏、ImGui 面板）。以下为性能与架构升级储备。

- [ ] Log / 日志系统 | 双缓冲队列在 >1000 条/帧时存在锁竞争，需引入 TLS 无锁写缓冲（UE TraceLog 模式），每个线程 `thread_local` 预分配 64KB 环形缓冲，三指针模型（Cursor/Committed/Reaped），后台 Worker 定期批量收集，消除锁竞争。
- [ ] Log / 日志系统 | 当前日志为纯文本消息，AI 无法按字段过滤（如 `fps < 30`），需引入结构化字段日志（Bevy tracing 风格），`LOG_STRUCT("Render", "frame_stats", field("fps", fps), field("draw_calls", dc))`，JSON 输出增加 `fields` 对象。
- [ ] Log / 日志系统 | 掉帧排查时日志与帧时间、实体数、内存用量未关联，需每帧自动注入 `DiagnosticSnapshot`（fps / frame_time_ms / entity_count / memory_used_mb），`LOG_STRUCT("Diagnostics", "frame_snapshot", ...)`。
- [ ] Log / 日志系统 | 崩溃时未刷盘的日志会丢失，需注册 `std::set_terminate` 和信号处理函数，崩溃时无锁强制遍历所有缓冲区 `Committed` 数据写入文件。
- [ ] Log / 日志系统 | 当前 `Logger` 为全局单例，与 ECS 架构不一致，需重构为 ECS Event 驱动：`LogEvent` 瞬时 ECS Event + `LogSink` Component（Console / File / JsonFile）+ `LogFilter` Resource，`ConsoleSinkSystem` / `FileSinkSystem` / `JsonSinkSystem` 并行消费。

## Module / 模块架构
> 来自已完成的模块重构计划，以下拆分/扩展时机未到，但方向已明确。

- [ ] Module / 模块架构 | 当前 `window/` 只含窗口和输入，未来需加入线程池抽象、文件 IO 底层、网络 Socket、CPU/SIMD 硬件检测、动态库加载，需扩展为 `platform/` 下设 `window/`、`input/`、`thread/`、`filesystem/`、`network/` 子目录。
- [ ] Module / 模块架构 | 反射系统（atom_registry、type_registry、inspector_reflection）与资源管理（prefab、scene_serializer）已独立为多个文件，文件数 > 5，值得独立，需从 `core/` 拆分出 `reflect/` 和 `asset/`，`reflect/` 负责类型注册/属性遍历/序列化/Inspector 自动生成，`asset/` 负责 Handle/异步加载/引用计数/热重载。
- [ ] Module / 模块架构 | `core/` 与具体业务之间缺少"比 core 更业务、比 gameplay 更通用"的层，需引入 `common/` 通用中间件层，包含场景图（Transform 层级脏标记传播）、Prefab 资产结构、序列化框架、状态机基础、调试绘制接口。
- [ ] Module / 模块架构 | `render/` 下所有文件平铺，随着 RHI、材质、后处理加入会越来越混乱，需按 `render/rhi/`（RHI 抽象层 + 各后端）、`render/renderer/`（RenderGraph、RenderPass）、`render/material/`（Material、ShaderCache）、`render/2d/`（Sprite、UI）、`render/post_process/`（后处理栈）分层。
- [ ] Module / 模块架构 | `math/aabb.h:42` 注册了 ECS 组件 `REFLECT_COMPONENT(AABB)`，迫使 `math` 模块依赖 `core/type_registry.h`，破坏底层纯净性，需在 `render/components/` 下新建 `WorldAabb.h`（主世界）与 `RenderAabb.h`（渲染世界）作为专用 ECS 组件，`math/aabb.h` 移除 `type_registry.h` 依赖，恢复零依赖。

## Core Runtime / 剩余优化
> Phase 4 核心运行时闭环已大部分落地，以下为代码中仍存的 legacy 与缺失。

- [ ] Core Runtime | `imgui_panels.cpp` 中仍有 `Fallback: legacy ComponentDesc recursive lookup` 分支，新增组件若未走新反射路径会静默回退到旧逻辑，需补全 `AtomRegistry::registerBuiltinAtoms()` 覆盖所有引擎内置原子类型，`imgui_panels.cpp` 中删除 legacy 分支，强制走 `inspectorDrawComponent()` 递归绘制。
- [ ] Core Runtime | `App::addPlugin()` 只有唯一性检查，无依赖声明与拓扑排序，若 Plugin A 依赖 Plugin B 但 B 未注册只能在运行时才发现功能缺失，需在 `IPlugin` 中增加 `dependencies()` 虚函数返回依赖名称列表，`App` 在 `build()` 前做拓扑排序，循环依赖或未满足依赖时 `LOG_FATAL`。
- [ ] Core Runtime | `ViewBinnedPhases` / `ViewSortedPhases` / `ViewVisibleList`（`render/RenderResources.h`、`render/culling/ViewVisibleList.h`）未注册 `REFLECT_COMPONENT`，Inspector 和序列化系统无法遍历字段，需补全注册，并确认 `ViewVisibleList` 中的 `DynamicArray<Entity>` 反射系统是否支持容器字段（当前可能只支持 Atom/Composite）。
