# SelfGameEngine TODO / 技术债务

## ECS / Core Runtime
- [ ] ECS / Core Runtime | `src/world.h` 中 `std::vector<bool> alive` 存在位压缩特化性能陷阱，读写较慢且无法返回真实引用，需替换为 `std::vector<uint8_t>` 或引入 Sparse Set 架构。
- [ ] ECS / Core Runtime | `src/world.h` 中 `positions` / `velocities` 数组只增不减，先 spawn 大量实体再销毁后数组仍保持峰值大小，浪费内存，需升级为 Sparse Set（`dense + sparse`）让存活数据保持紧凑。
- [ ] ECS / Core Runtime | `AgentBridge`（`_engine/source/bridge/agent_bridge.cpp:12`）越界承担 System 注册职责，直接持有 `World`、`Scheduler`、`MovementSystem` 并在 `init()` 中注册，把「AI 接口桥梁」与「运行时装配」耦合在一起，需将装配逻辑上提到 Runtime / App 层，`AgentBridge` 仅通过指针/引用访问外部注入的世界与调度器。
- [ ] ECS / Core Runtime | `ecs/type/type_registry.h` `allocateNextID()` 使用 `u64` 掩码硬限制 64 种组件类型（`CHECK(m_next_id < 64)`），当前约 10~15 个但渲染/物理/动画组件加入后将触顶，需迁移到 Archetype/Chunk 存储或改用 `DynamicArray<u64>` 位集。
- [ ] ECS / Core Runtime | `ecs/world/world.h` `m_component_arrays` 使用 `HashMap<ComponentTypeID, IComponentArray*>` 裸指针存储，无分配器注入，生命周期由 `World` 析构手动 `delete`。
- [ ] ECS / Core Runtime | ECS 当前没有 Resource 概念（全局数据存储），`Assets<T>` 是独立存储，后续 ECS 演进后应把 `Assets<T>` 注册为 World 级全局数据。
- [ ] ECS / Core Runtime | `ecs/world/plugin.h` `PluginManifest` 只记录 name/phase/dependencies，缺少 registered_components / registered_systems / registered_resources，AI Agent 无法完整查询插件能力。

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
> 2026-05-31：已完成级别 1 轻量优化（调试标注 + Uniform Cache + 错误码骨架）。

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
- [x] Render / RHI | `FrustumCullSystem`（`render/culling/FrustumCullSystem.cpp:37`）与 `QueueDrawsSystem`（`render/queue/QueueDrawsSystem.cpp:31`）每帧通过 `Query<ViewVisibleList>` / `Query<ViewBinnedPhases>` / `Query<ViewSortedPhases>` 遍历全部实体来定位唯一的 Resource-like 组件，且不存在时会创建新实体，导致 render world 实体数只增不减，需将 View-level Resource 绑定到 view 实体或引入 ECS 的 Resource 系统（非组件的全局表）。
  - 完成：2026-06-12，`ExtractCameraSystem` 创建 view 实体时同时预绑定 `ViewVisibleList` / `ViewBinnedPhases` / `ViewSortedPhases`；`FrustumCullSystem` 与 `QueueDrawsSystem` 改为在 view 实体上 O(1) 查取。新增 `RenderViewResources` 单元测试验证每帧仅剩 1 个 view 实体。
- [ ] Render / RHI | `BinnedRenderPhase`（`render/queue/BinnedRenderPhase.h`）使用 `DynamicArray<PhaseBin>` 线性分箱，因 ECS `ComponentArray::set` 要求组件可拷贝而 `HashMap` 显式 delete 了拷贝，为绕过限制改用线性查找，大规模场景下退化为 O(n)，需恢复 `HashMap` 或引入独立 Resource 系统。
- [ ] Render / RHI | `QueueDrawsSystem`（`render/queue/QueueDrawsSystem.cpp:65`）深度计算使用物体原点 `worldMatrix * Vec3{0,0,0}` 的 z 值，对于中心偏移的大型物体排序结果不准确，若实体有 `AABB` 应使用 `(aabb.min + aabb.max) * 0.5f` 作为深度采样点，无 AABB 则回退到原点。
- [x] Render / RHI | `QueueDrawsSystem`（`render/queue/QueueDrawsSystem.cpp:48`）SortKey 的 float→uint 编码未处理 viewZ ≤ 0，IEEE-754 负数 bit pattern 按 uint 排序与数值排序不一致，若物体在相机后方或近平面处深度键会混乱，需将 viewZ 钳制到 `[near, far]` 后规范化到 `[0, 1]` 再编码为 uint（如 `uint32_t(depth * 0xFFFFFFFF)`），保证全范围单调。
  - 完成：2026-06-12，`ExtractedView` 增加 `near_plane` / `far_plane`；`QueueDrawsSystem` 改用 `encodeLinearDepth` 将 viewZ 规范化到 `[0, 1]` 后编码为单调 uint；透明/UI 仍通过按位取反实现远→近排序。新增 `SortKeyDepthEncoding` 单元测试覆盖负深度钳制、近平面/远平面边界、单调性与透明降序。
- [ ] Render / RHI | `FrustumCullSystem` 和 `QueueDrawsSystem` 由调用方手动按顺序调用，无内建依赖声明，未来渲染步骤增多后容易顺序出错，需仿照 `ExtractSchedule` 引入 `RenderSchedule`（`IRenderSystem` 接口 + 注册表），在 `RenderWorld` 中统一定义 Extract → Cull → Queue → ... 的 SystemSet 链。
- [ ] Render / RHI | Culling 与 Queue 系统仅处理第一个 `ExtractedView`。`ViewVisibleList`、`ViewBinnedPhases`、`ViewSortedPhases` 已绑定到 view 实体（单视图完成）；多视图扩展时需遍历所有含 `ExtractedView` 的实体，为每个 view 生成独立的可见列表和 phase 容器。
- [ ] Render / RHI | `PhaseItem::instance_count`（`render/queue/PhaseItem.h`、`render/queue/QueueDrawsSystem.cpp`）字段已预留为 1 但未实现 instancing 合并，`QueueDrawsSystem` 未检测同 material + 同 mesh 的连续实体，需在 `BinnedRenderPhase::addItem` 中检测并合并为同一 `PhaseItem` 且累加 `instance_count`，并配合 Prepare 步骤生成 instance buffer。
- [ ] Render / RHI | `ExtractRenderablesSystem`（`render/extract/ExtractRenderablesSystem.cpp:23`）每帧对静态 AABB 全量拷贝，大多数模型的本地 AABB 是静态的但每帧都通过 `mainWorld.getComponent<AABB>(entity)` 提取到 render world，需在 `MainWorldSync` 中记录「上一帧是否有 AABB」或引入脏标记机制，仅当 AABB 组件被修改时才重新提取。
- [ ] Render / RHI | `IRHICommandList::setUniform*` 仍为 OpenGL immediate mode（`glUniform*`），虽已引入 Uniform Location 缓存消除字符串查询，但每 Draw Call 仍单独调用驱动，无法合批。未来应迁移到 UBO / PushConstants / Bindless。

## Material / Shader
> 2026-05-25：当前实现为同步编译 + CPU uniform 块 + `glUniform*` 即时上传 + 无模板分层。以下为与工业级方案的差额，后续逐步补齐。

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
- [ ] Base Layer | `StringInternPool` ECS Resource 化。当前单 World 场景全局单例无问题，阶段 4 多 World 或编辑器需要隔离时实施。
  - 参考：知识库 `Notes/SelfGameEngine/基础工具层/字符串系统.md` — Intern 池全局状态应以 ECS Resource 形式存在
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
- [ ] Module / 模块架构 | `render/` 下所有文件平铺，随着 RHI、材质、后处理加入会越来越混乱，需按 `render/rhi/`（RHI 抽象层 + 各后端）、`render/renderer/`（RenderGraph、RenderPass）、`render/material/`（Material、ShaderCache）、`render/2d/`（Sprite、UI）、`render/post_process/`（后处理栈）分层。
- [ ] Module / 模块架构 | `math/aabb.h:42` 注册了 ECS 组件 `REFLECT_COMPONENT(AABB)`，迫使 `math` 模块依赖 `core/type_registry.h`，破坏底层纯净性，需在 `render/components/` 下新建 `WorldAabb.h`（主世界）与 `RenderAabb.h`（渲染世界）作为专用 ECS 组件，`math/aabb.h` 移除 `type_registry.h` 依赖，恢复零依赖。

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

## Build System / 构建体系重构
> 2026-05-30：完成 Phase 1 — 引入 `entelechy_module()` 宏、去掉代理层、统一模块声明、CMake 直接驱动模块发现。
> 2026-05-30：完成 Phase 2 — 清理模块边界，去掉 `..` PUBLIC 暴露，统一跨模块 include 为裸文件名风格，修复隐式依赖。

- [ ] Build System | `test_runner/CMakeLists.txt` 使用 `$<TARGET_OBJECTS:*Tests>` 收集测试对象文件，该 generator expression 在部分 CMake 生成器或平台上行为不一致，若未来切换到 Ninja/Make 需验证兼容性。
  - 文件：`_engine/source/test_runner/CMakeLists.txt`
- [ ] Build System | `launch/generator.py` 已标记弃用但尚未移除，作为 standalone `main.cpp` 生成的 fallback 保留。待团队完全切换到纯 CMake 流程后删除。
  - 文件：`launch/generator.py`
- [ ] Build System | `main.cpp.in` 模板中硬编码了大量跨模块 include（如 `#include "glfw_window.h"`、`#include "render/opengl_backend.h"`），未通过模块依赖自动推导。未来应让各模块在 `entelechy_module()` 中声明「需要暴露给 main 的头文件」，或由 CMake 自动生成 include 列表。
  - 文件：`launch/templates/main.cpp.in`

## ThreadPool / 线程池

- [ ] ThreadPool / 线程池 | `thread_pool/public/thread_pool/thread_pool.h` `WorkStealingQueue` 固定容量 4096，本地队列满时回退到全局 `overflowMutex` + `std::deque`，极端负载下可能阻塞，需评估是否引入动态扩容或更优溢出策略。

## VFS / 虚拟文件系统

- [ ] VFS / 虚拟文件系统 | `vfs/private/mount_point.cpp` `FileSystemMountPoint` 使用 `fopen/fread/fwrite` 做文件 IO，路径拼接限制 512 字节缓冲区，未来应支持超长路径和异步 IO。

## Window / 窗口系统

- [ ] Window / 窗口系统 | `window/public/window/window/window.h` `IWindow` 目前只有 GLFW 实现，未来需加入 SDL / Win32 后端。
- [ ] Window / 窗口系统 | `window/public/window/window/glfw_window.h` `getNativeDisplay()` 是 Vulkan 创建 Surface 的预留 stub，未实现。

## Runtime / 游戏运行时

- [ ] Runtime / 游戏运行时 | `_game/source/runtime/private/game_runtime.cpp` `main.cpp` 由 `launch/generator.py` 构建时生成，主循环逻辑散落在模板中，未来 Runtime 应接管更多主循环逻辑。

## Core / 基础库扩展缺口

> 2026-05-31：当前代码已按「基础库优先」规则消除了所有可替换的 STL/裸分配依赖。以下设施因基础库尚未提供对应实现，暂时保留 STL，待扩展后统一迁移。

- [ ] Core / 基础库扩展缺口 | `std::unique_ptr<T>` 仍在 `log/core/logger.h`（`addOutputDevice` / `m_devices`）与 `render/example/simple_cube_renderer.h`（`m_device`、`m_shader_cache`）中使用。需引入引擎级 `UniquePtr<T, Deleter>`（支持自定义 `DefaultAllocator` 释放），并迁移所有所有权语义场景。
- [ ] Core / 基础库扩展缺口 | `std::function<void()>` 仍在 `thread_pool/thread_pool.h`、`asset/loader/asset_server.h`、`bridge/tool_registry.h` 中使用。需设计零分配或固定缓冲的 `Function<Ret(Args...)>` / `Delegate`（参考 `ue::TFunction` / `bevy::Func`），避免 `std::function` 的类型擦除堆分配与不可拷贝约束。
- [ ] Core / 基础库扩展缺口 | `std::deque<T>` 仍在 `thread_pool` 溢出队列与 `asset_server` 任务队列中使用。需实现支持头尾 O(1) push/pop 的 `Deque<T>`，或更直接地提供 lock-free `MPSCQueue<T>` 替换线程池/资源加载的回调队列。
- [ ] Core / 基础库扩展缺口 | `std::sort` 仍在 `render/queue/SortedRenderPhase.cpp` 中使用。需引入 `algo::sort(begin, end, cmp)`（可考虑 introsort / timsort），并对 `DynamicArray` 提供 convenience 成员方法。
- [ ] Core / 基础库扩展缺口 | `std::thread` / `std::mutex` / `std::atomic` / `std::condition_variable` 仍散落在线程池、asset_server、logger 中。需封装为 `Thread`、`Mutex`、`Atomic<T>`、`ConditionVariable` 等薄层，便于未来切换平台线程模型（如 Windows ThreadPool API、C++20 `std::jthread`）。

## Allocator / ECS 存储优化（2026-05-31 完成阶段一至四基础实现，以下待后续细化）

- [ ] Allocator / ECS 存储优化 | `ecs/public/ecs/archetype/archetype_world.h` `ArchetypeWorld` 目前与 `World`（SparseSet+Column）并存，未实现双轨迁移路径。`ArchetypeWorld` 缺少 `setParent`、批量 `spawn`、事件系统、`CommandBuffer` 等能力，无法直接替换现有 `World`。需在 `App` / `Scheduler` 中支持可选的 world 后端，逐步将 System 从 `World` 迁移到 `ArchetypeWorld`。
  - **阻塞点**：`System::tick(World&, ...)` / `Scheduler::tick(World&, ...)` / `CommandBuffer::apply(World&)` 签名硬编码旧 `World`；`Query<Cs...>` 模板依赖 `World::getComponentArray`；`AgentBridge` 直接遍历 `world.componentArrays()`；渲染系统的 `IExtractSystem` 接口也硬编码 `World&`。迁移需跨 `ecs` / `motor` / `render` / `bridge` / `_game` 多个模块，非 ECS 模块内部可独立完成。
- [ ] Allocator / ECS 存储优化 | `test_runner/CMakeLists.txt` 依赖 `entelechy_get_enabled_modules` 收集测试 OBJECT 库，但各模块 `tests/CMakeLists.txt` 使用裸 `add_library(... OBJECT)` 而非 `entelechy_module()`，导致测试目标无法被自动发现。当前 workaround 是在每个 `tests/CMakeLists.txt` 末尾手动 `set_property(GLOBAL APPEND PROPERTY ENTELECHY_ENABLED_MODULES ...)`，应统一改用 `entelechy_module(NAME XxxTests TYPE OBJECT NO_TESTS)`。
