# Entelechy Render 层实现进度 —— 逐节对照报告

> **生成日期**: 2026-08-04  
> **对照基准**: Obsidian 知识库 `SelfGameEngine/渲染管线与画面/` 笔记（阶段 5 全部 20+ 篇）  
> **工程路径**: `_engine/source/render/` + `_engine/source/render_system/` + `_engine/source/asset/`  
> **总体评估**: 核心骨架已建立（5.1/5.3/5.4/5.6），但超过一半的模块尚未启动实现

---

## 阅读指南

每节包含四个维度：

| 维度 | 含义 |
|------|------|
| **知识库设计** | Obsidian 笔记中定义的架构、关键接口、实现要求 |
| **代码现状** | 工程中实际存在的文件和代码 |
| **缺失项** | 对照笔记，代码中尚未实现的模块/接口/功能 |
| **问题/风险** | 设计偏离、硬编码、债务项、集成断裂点 |

---

## 5.1 RHI 抽象层与命令模型

**知识库设计**：中厚度自研 RHI，接口对齐 UE `FRHICommandList`。核心接口 `IRenderDevice`（资源工厂 + Submit/Present）+ `ICommandContext`（Begin/EndRenderPass、绑定、Draw、Barrier）。命令模型选择"延迟命令缓冲"——`RenderCommandBuffer` + `LinearAllocator` 每帧分配命令内存，`RenderCmdHeader` + 命令枚举分派执行。资源标识采用 `Handle<Tag>`（index+generation）+ `ResourceTable<T,Tag>`（dense array + free list）。**首个后端 = D3D12**（隐式状态机 OpenGL 只在笔记中用于学习对比，不做引擎后端）。

**代码现状** ✅ 多后端接口对齐完成（6a），延迟命令缓冲完成（6b），D3D12 后端落地（6d），GL/D3D12 双后端功能等价

已实现：
- `IRHIDevice` 统一接口（合并原 `IRenderBackend`）：surface 生命周期（`initSurface`/`shutdownSurface`/`beginFrame`/`endFrame`/`setClearColor`/`clear`/`resizeSurface`）+ 资源工厂 + 命令提交 + 帧 fence + 延迟删除 + 内存追踪 → `render/public/rhi/rhi_device.h`
- `IRHICommandList` 纯虚接口：`beginRenderPass()`/`endRenderPass()`、viewport/scissor、绑定管线/顶点/索引/纹理/Uniform、`drawIndexed()`/`draw()`、resource barrier、debug group、`bindConstantBuffer()`/`setPushConstants()` → 同上
- **延迟命令缓冲（6b）**：`RenderCommandBuffer` 连续内存 bump allocator（4 MB 默认）+ `RenderCommandType` 枚举（24 种命令）+ payload 结构体 → `render/public/rhi/render_command_buffer.h/.cpp`；`DeferredCommandList` 实现 `IRHICommandList` 纯序列化 → `render/public/rhi/deferred_command_list.h/.cpp`；`GLCommandTranslator` 遍历 buffer 翻译为 GL 调用（含状态缓存）→ `render/public/rhi/gl_command_translator.h/.cpp`；`GLRHIDevice` 通过 pimpl `DeferredState` 持有三者，`createCommandList()` 返回 recorder，`submit()` 驱动 translator.execute()
- **D3D12 后端（6d）**：`D3D12RHIDevice` 实现 `IRHIDevice` 全接口（DXGI 1.6 + flip-model swapchain 2 帧 + 自有 D32 深度 + 2 帧 in-flight allocator 轮换 + `IDXGIAdapter3` 显存查询 + backbuffer readback）→ `render/public/rhi/d3d12_rhi_device.h` + `render/private/rhi/d3d12_rhi_device.cpp`；`D3D12CommandTranslator` 复用同一 `RenderCommandBuffer` 翻译为 D3D12 调用：全局 root signature（CBV b0..b2 按 stage 拆分 slot 0-5 + SRV 表 t0..t2 slot 6 + 3 静态 sampler），cbuffer 布局由 DXC 容器反射（`IDxcContainerReflection`，非旧 `D3DReflect`——不支持 DXIL）在 PSO 创建时获得，`setUniform*` 展平名解析回 (cbuffer, vec4 索引) 经 CPU staging → per-frame upload ring → root CBV；纹理 SRV 持久 CPU heap（free-list）→ 每帧 copy 进 shader-visible heap；纹理上传时行翻转（cooked UV 的 V 翻转是 GL 底左原点的几何侧约定）→ `render/public/rhi/d3d12_command_translator.h` + `render/private/rhi/d3d12_command_translator.cpp`
- 后端选择：`--backend=<gl|d3d12>` / `ENTELECHY_BACKEND` 环境变量，默认 OpenGL；`shaderFileExtensionForBackend()`/`shaderFormatForBackend()` 按后端选着色器产物（`.glsl`/`.dxil`/`.spv`）→ `render/private/rhi/rhi_device_factory.cpp`
- `IRHIFence` 独立 GPUResource 子类：`signal()`/`wait()`/`getCompletedValue()`/`isSignaled()`；`GLFence` 底层 GLsync、`D3D12Fence` 底层 `ID3D12Fence` + event → `public/rhi/rhi_resources.h` + `gl_rhi_device.h` + `d3d12_rhi_device.h`
- `ShaderBytecode` 结构体：`{stage, format(GLSL/SPIRV/DXIL), data, size, entryPoint}`；`createShader()` 接受多格式字节码 → `public/rhi/rhi_types.h`
- `ResourceState` 位掩码枚举（Common/VertexBuffer/IndexBuffer/ConstantBuffer/ShaderResource/UAV/RenderTarget/DepthWrite/DepthRead/CopySrc/CopyDst/Present）+ resource-referencing `BarrierDesc` → `public/rhi/rhi_types.h`；D3D12 后端翻译为 `D3D12_RESOURCE_BARRIER`（6d）
- `RenderBackendType` 枚举（OpenGL / D3D12 / Vulkan）+ `createRHIDevice()` 工厂函数 → `public/rhi/rhi_device_factory.h`
- `PipelineStateDesc` 含顶点输入布局（`vertexStride` + `vertexAttributes[8]`，6d 起；GL 忽略，D3D12 烘焙进 PSO，attribute location→HLSL 语义名映射在 D3D12 后端内）→ `public/rhi/rhi_pipeline.h`
- `ClearFlags` 位掩码（Color/Depth/Stencil）→ `public/rhi/rhi_types.h`

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| ~~延迟命令缓冲~~ | ~~`RenderCommandBuffer` + `LinearAllocator` + `RenderCmdHeader` 命令枚举分派~~ | ✅ 已完成（6b）。专用 bump allocator 替代通用 LinearAllocator |
| `Handle<Tag>` + `ResourceTable` | RHI 层本身的资源标识用 tagged handle + dense array | ❌ 不存在于 RHI 层。RHI 用裸 `RHIRef<T>` 智能指针 |
| ~~D3D12 后端~~ | ~~笔记明确："首个后端 = D3D12"~~ | ✅ 已完成（6d，2026-08-07）。Sponza 画面与 GL 等价；已知限制见 TODO（mip 生成/同步上传/PIX/ImGui-DX12） |
| `ErrorPolicy` / `RHIErrorCode` | 统一错误处理（DEBUG assert、Release log） | ❌ 不存在 |
| UBO/CBV 统一绑定 | `bindConstantBuffer`/`setPushConstants` 已有接口但无调用方 | ⏳ 接口就绪，6e 落地实际绑定路径 |

**问题/风险**：
- `setUniform*` 仍为 per-draw GL 语义——新 `bindConstantBuffer`/`setPushConstants` 接口已就位，6e 迁移 Material::bind()
- AGENTS.md 明确记录债务："GLRHIDevice 不是 ECS Resource，渲染系统直接调 GL"——应通过 `IRHICommandList` 路由

---

## 5.2 多线程命令录制与并行渲染 ⭐

**知识库设计**：三层并行化（Generate → Translate → Submit）。Phase 1 单线程录制+翻译；Phase 2 并行 `DrawPacket` 生成（线程局部缓冲 + 全局归并）；Phase 3 并行翻译（range-slice + command-list 数组 + 有序 submit）。`DrawPacket` 是纯数据（不含 API handle），`SortKey` 64-bit 排序键。

**代码现状** ✅ 大部分完成（CPU 侧并行生成已实现，GPU 命令并行录制未实现）

已实现：
- `PhaseItem` 即 `DrawPacket` 等价物 → `render_system/public/queue/PhaseItem.h`
- 三层并行化在 `FrustumCullSystem::run()` 和 `QueueDrawsSystem::run()` 中实现：
  - 并行分片 → 每 worker 线程局部 `DynamicArray` 缓冲
  - 原子批次计数器 `nextBatch.fetch_add`
  - Busy-wait barrier `completedTasks`
  - 确定性批次顺序归并
- `ThreadPool`（Chase-Lev work-stealing deques）→ `thread_pool/public/thread_pool.h`
- >256 实体时自动启用并行路径
- 测试验证 serial == parallel 等价 → `render_system/tests/test_render_parallel.cpp`

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| Phase 3 并行翻译（GPU 命令录制） | command-list 数组 + range-slice + 有序 Submit | ❌ 只有 CPU 侧并行 generation。`GLCommandList` 仍是单线程立即执行 |
| `DrawPacket`（纯数据，无 API handle） | 与 `PhaseItem` 语义一致 | ✅ 等价存在 |
| RHI Thread 分离 | Game Thread + Render Thread + RHI Thread 三级 | ❌ 仅 Game Thread + 单线程 Render |

**问题/风险**：当前的"并行"是 CPU 侧数据生成并行，GPU 命令录制仍是单线程立即模式。这对 OpenGL 是正确的（GL 上下文线程绑定），但对 D3D12/Vulkan 预留的 command-list 并行录制能力尚未验证。

---

## 5.3 GPU 资源生命周期管理 ⭐

**知识库设计**：引用计数 + 延迟删除队列（Fence 门控）、`FrameRingBuffer`（分段 + Fence）、`GPUMemoryBudget`（budget/usage/threshold + 软预算压力降级）、`TransientResourcePool`（Ring Buffer/池复用）、帧上下文批量清理（Granite 风格）、ECS 化表达（Branch C：不可变后端单例 + 可变 ECS Resource）。

**代码现状** ✅ 大部分完成（核心机制齐全，缺少预算强制和 ECS 化）

已实现：
- `GPUResource` 原子引用计数 + `RHIRef<T>` 智能句柄 → `public/rhi/rhi_resources.h`
- Fence 门控延迟删除：`signalFrame()`/`getCompletedFenceValue()`/`queueResourceForDelete()`/`flushPendingDeletes()` → `GLRHIDevice`（GL sync object `GL_SYNC_GPU_COMMANDS_COMPLETE`）+ `D3D12RHIDevice`（6d：`ID3D12Fence` 单时间线 + event 等待，2 帧 in-flight command allocator 轮换）
- D3D12 堆模型（6d）：default heap + 专用 upload list 资源上传、8MB per-frame upload ring（root CBV 常量）、256 槽持久 CPU SRV heap（free-list）+ 4096 槽 per-frame shader-visible SRV heap（bump）
- 内存预算查询：`queryMemoryInfo()`（GL: NVX/ATI 扩展；D3D12: `IDXGIAdapter3::QueryVideoMemoryInfo`）+ `getTrackedMemoryUsage()` + 每资源 `memorySizeBytes()`
- `TransientTexturePool`：按描述符分组复用 + Fence 保护回收 + `acquire`/`release`/`purgeCompleted` → `public/rhi/rhi_transient_resource_pool.h/.cpp`
- `FrameArenaRing<2>` 在 RenderWorld 中用作帧环形缓冲区 → `render_world/RenderWorld.h`
- 测试覆盖：延迟删除、引用计数句柄、transient pool、内存大小计算 → `tests/test_gpu_resource_lifecycle.cpp`

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| `FrameRingBuffer` 独立类 | 分段 + Fence 的通用环形缓冲 | ❌ 无独立类。RenderWorld 有 `FrameArenaRing<2>` 但不含 Fence 语义 |
| `GPUMemoryBudget` 预算强制 | 软预算 + warning + 压力降级 | ❌ 无预算强制。内存仅被动追踪 + best-effort 查询 |
| `TextureTable` 作为 ECS Resource | 纹理表作为世界资源 | ❌ 不存在。AGENTS.md 记录此为已知债务 |
| Transient 内存别名 | RenderGraph 驱动的瞬态资源别名复用 | ❌ `TransientTexturePool` 仅全纹理复用，不做别名 |

**问题/风险**：
- 资源生命周期管理在 `GLRHIDevice` 中作为单例实现，不是 ECS Resource——AGENTS.md 标记为待迁移
- 内存预算只查询不强制——没有 OOM 防御策略

---

## 5.3b PSO 缓存与异步编译 ⭐

**知识库设计**：`PipelineStateDesc` + hash/cmp、`PSOManager`（HashMap + read/write lock + double-check）、`AsyncPSOJob`（desc + future + placeholder + state）、异步编译线程池（idle priority）、placeholder fallback（最近缓存 PSO 或 error-pink）、D3D12 Pipeline Library / Vulkan PipelineCache 磁盘序列化 + 版本校验、`PSOManagerResource` ECS Resource、`PSOCompileSystem`。

**代码现状** 🟡 部分完成（仅同步缓存）

已实现：
- `PipelineStateDesc` + `std::hash` 支持 → `public/rhi/rhi_pipeline.h`
- `PSOManager`：全局 HashMap 缓存 + 统计信息 → 同上
- 同步 `getOrCreate()` → 同上
- PSO 缓存接线（阶段 5c，2026-08-06）：`GLRHIDevice::createPipelineState` 经 `PSOManager::find`/`insert` 走缓存——此前 `Material::init` 直调设备创建、PSOManager 是死代码；相同 shader pair + 状态现在共享一个 GL program，Sponza 28 材质 + fallback + 天空实测收敛为 3 个 PSO（单面 / 双面 / 天空），缓存大小经 `FrameStats` 入 Render Stats 面板

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| 异步编译 | `AsyncPSOJob` + 线程池 + placeholder fallback | ❌ 不存在。header 注释："Phase 2 (future)" |
| Pipeline Library 磁盘缓存 | D3D12 PSO Library / Vulkan PipelineCache 序列化 | ❌ 不存在 |
| `PSOManagerResource` ECS | PSO 管理器作为 ECS Resource | ❌ 不存在。`PSOManager` 是 `GLRHIDevice` 的直接成员 |
| `PSOCompileSystem` | ECS System 驱动异步编译 | ❌ 不存在 |

---

## 5.4 ECS 架构下的渲染世界设计 ⭐

**知识库设计**：逻辑-渲染并行（延迟帧管线渲染）、双 World 模型（Main World + Render World）、Extract 边界（只读快照语义）、跨 World 实体映射（`MainWorldSync`）、完整帧生命周期 Extract → PrepareAssets → Culling → Queue → RenderGraph → Execute/Submit、Render World 作为确定性投影。

**代码现状** ✅ 大部分完成（核心架构 + 帧驱动层齐全，缺少 Prepare 阶段）

已实现：
- 双 World 模型：主 `World` + 独立 `RenderWorld`（含自己的 ECS `World`、`ExtractSchedule`、`MainWorldSync`、双缓冲帧 arena）→ `render_world/RenderWorld.h/.cpp`
- `IExtractSystem` 接口 + 顺序 `ExtractSchedule`（单线程）→ `render_world/ExtractSchedule.h/.cpp`
- 跨 World 实体映射：`MainWorldSync` 含双向 `main_to_render`/`render_to_main` HashMap → `private/extract/MainWorldSync.h`
- `ExtractRenderablesSystem`：拷贝 `(MeshAssetRef, MaterialAssetRef, GlobalTransform, 可选 WorldAABB)` → `(RenderMesh, RenderMaterial, RenderTransform, 可选 RenderAABB)` → `private/extract/ExtractRenderablesSystem.cpp`
- `ExtractCameraSystem`：拷贝第一个 `Camera` → `ExtractedView`，预绑定 view resources，使用 `IWindow` 获取 aspect/viewport → `private/extract/ExtractCameraSystem.cpp`
- 组件：主 World `MeshAssetRef`/`MaterialAssetRef`/`Camera`；Render World `RenderMesh`/`RenderMaterial`/`RenderTransform`/`ExtractedView` → `public/components/`
- `RenderFrameRunner`：生产级帧驱动层，每帧串联 Extract → Cull → Queue → Execute，并产出 `FrameStats` → `frame/RenderFrameRunner.h/.cpp`

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| Prepare 阶段 | 异步资源解析（Handle → GPU 资源） | ❌ 不存在。Extract→Cull→Queue 链路中缺少 Prepare（阶段1 由 main 手工注册 GPU 网格/材质代替） |
| 完整帧生命周期驱动 | `RenderSystem::runFrame()` 编排全流程 | ✅ `RenderFrameRunner::runFrame()`（2026-08-04，阶段1） |
| 并行 Extract | 多线程 Extract（笔记明确："初始顺序 → 并行 → RHI thread"） | ❌ 当前仅单线程顺序 ExtractSchedule |
| RenderGraph 集成 | 笔记要求 RenderGraph 作为 RenderWorld 的 Resource | ❌ RenderGraph 完全不存在（见 5.7） |

**问题/风险**：
- Extract 链路与 GPU 消费端已由 `RenderFrameRunner` 串联并在主循环中运行；Prepare 阶段仍缺失
- 并行 Extract / RenderGraph 仍未开始

---

## 5.5 可见性判断与空间加速结构 ⭐

> 知识库拆分为上篇（视锥剔除与空间加速结构）和下篇（遮挡剔除与 GPU-Driven 渲染）

**知识库设计**：

上篇（视锥剔除）：包围盒选择（AABB 默认 + Sphere 预过滤）、6 平面视锥测试（front p-vertex）、O(n) 剔除瓶颈 → 空间加速（Uniform Grid / Octree / BVH）、BVH 构建（Top-down SAH / LBVH）、Static BVH + Dynamic List 双轨制。

下篇（遮挡剔除）：HZB 遮挡剔除 + 两遍策略、GPU-Driven L1~L5 分级（L1 Indirect Instance → L2 GPU Compute Cull + DrawIndirect → L3 Mesh Clusters + MultiDrawIndirect + Bindless → L4 Visibility Buffer / Nanite → L5 Mesh Shader）。

**代码现状** 🟡 部分完成（仅视锥剔除基础版）

已实现：
- `Frustum`（planes[6]）+ `FrustumFromMatrix` → math 库
- `FrustumCullSystem`：逐实体 AABB vs `ExtractedView.frustum` 视锥剔除，并行路径（>256 实体 + ThreadPool）→ `private/culling/FrustumCullSystem.cpp`
- `ViewVisibleList` 输出 → `phase/ViewVisibleList.h`（header 注释明确："CPU brute-force frustum culling. Tomorrow: BVH"）
- 实跑验证（2026-08-05，阶段 3c）：cooked Sponza 405 实体全部携带世界 AABB（cooker 在 `scene.json` 输出 primitive 局部 AABB，游戏侧 `scene_loader` 做 8 角点×世界矩阵变换）；每秒 `Frame stats` 日志显示 `draw_calls == visible`、`culled` 随视角在 17~388/405 间实时变化，剔除统计生效

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| 空间加速结构（BVH/Octree/Grid） | SAH BVH + Static BVH + Dynamic List 双轨 | ❌ 不存在。`ViewVisibleList.h` 注释明确延迟 BVH |
| HZB 遮挡剔除 | Hi-Z Min-Mipchain + 两遍遮挡剔除 | ❌ 不存在 |
| GPU-Driven 渲染（L1-L5） | Compute shader cull + DrawIndirect + Bindless | ❌ 完全不存在 |
| SIMD 批量测试 | 笔记建议中期引入 SIMD 批量视锥测试 | ❌ 不存在 |

---

## 5.6 渲染队列与 DrawCall 组织 ⭐

**知识库设计**：64-bit `SortKey`（Phase|PSO|Material|Depth）、Binned Phase（不透明，HashMap 分箱，最小化 PSO 切换）+ Sorted Phase（透明，稳定深度排序远→近）、Instancing（箱内相同 mesh+material 自动升级为实例化绘制）、Phase Resource 作为显式 ECS Resource。

**代码现状** ✅ 大部分完成（核心机制齐全，Instancing 预留未实现）

已实现：
- 64-bit `SortKey` union：高 8-bit `phase`、中 16-bit `material_id`、低 32-bit `depth`。depth 使用 `encodeLinearDepth()` 单调 uint 编码（避免 IEEE-754 排序 bug）→ `public/queue/PhaseItem.h`
- `BinnedRenderPhase`（Opaque/AlphaMask）：`HashMap<u32, usize>` material→bin 索引 O(1) addItem → `public/queue/BinnedRenderPhase.h/.cpp`
- `SortedRenderPhase`（Transparent/UI）：稳定**基数排序**，透明 depth 取反（`~depthBits`）实现远→近 → `public/queue/SortedRenderPhase.h/.cpp`
- `QueueDrawsSystem`：逐 Phase 生成 PhaseItems，计算钳位深度，分箱/排序，并行路径 → `private/queue/QueueDrawsSystem.cpp`
- 输出容器 `ViewBinnedPhases`/`ViewSortedPhases` → `phase/RenderResources.h`
- 测试覆盖：serial vs parallel 等价、sort key 编码/解码、binned phase、sorted phase

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| Instancing 自动升级 | 箱内相同 mesh+material → 合并 instance_count | ❌ `instance_count` 字段存在但始终硬编码为 1（代码行70注释："reserved for instancing"） |
| ShadowMap Phase | ShadowMap 作为独立 RenderPhase | ❌ `QueueDrawsSystem` 中 ShadowMap 直接 `return false`（跳过） |

---

## 5.7 RenderGraph 与多 Pass 资源管理 ⭐

**知识库设计**：声明式 RenderGraph（声明 Pass 依赖 → 编译器自动推导 Barrier/拓扑排序/死 Pass 剔除/瞬态资源别名）、最小可行 RenderGraph 编译器（Pass 数组 + Resource 数组 + Kahn 拓扑排序 + 后向可达死 Pass 剔除 + 每资源 first-writer/last-reader Barrier 插入，<500 行）、transient 标记预留别名复用、frame index + Fence + SwapChain 三缓冲、默认单线程录制预留 RHI Thread。

**代码现状** ❌ 未开始

搜索全局代码：`RenderGraph`、`render_graph`、`FrameGraph`、拓扑排序、死 Pass 剔除、自动 Barrier——**全部不存在**。

`TransientTexturePool` 是全纹理复用池，不是 RenderGraph 驱动的内存别名系统。没有任何声明式 Pass+Resource 依赖图。

---

## 5.8 资源句柄与引用计数

**知识库设计**：ECS 不能存裸指针/智能指针 → `Handle<T>`（index+generation, 8-byte POD）、`AssetSlot<T>`/`AssetPool<T>`（slots + ref_counts + pending_free）、`OwnedHandle` RAII 包装器、`Assets<T>` ECS Resource、事件/Channel 驱动的 Drop（Bevy 风格 channel-drop）、增量/批量帧边界回收。

**代码现状** ✅ 大部分完成（在独立 `asset/` 模块中，但未与渲染管线集成）

已实现（在 `_engine/source/asset/` 中）：
- `Handle<T>`：index + generation、ABA 保护、8-byte POD、`std::hash` → `asset/public/handle/asset_handle.h`
- `HandleTable<T>`：dense 存储、free-list 复用、ref-count 并行数组、`allocate`/`fill`/`tryGet`（支持异步填充）→ `asset/public/handle/handle_table.h`
- `Assets<T>`：facade（AssetPool 等价），提供 `insert`/`allocateEmpty`/`fill`/`get`/`remove`/`refcount` → `asset/public/type/assets.h`

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| `OwnedHandle` | RAII 包装器，析构时 drop ref | ❌ 不存在 |
| 事件/Channel drop | SPSC/MoodyCamel queue 延迟回收 | ❌ 不存在 |
| 与渲染组件集成 | RenderMesh/RenderMaterial 应使用 `Handle<T>` | ✅ 已集成（2026-08-05，阶段 2a）：`MeshAssetRef`/`MaterialAssetRef`/`RenderMesh`/`RenderMaterial` 全部改用 `Handle<MeshAsset>`/`Handle<MaterialAsset>`，`RenderExecuteSystem` 注册表以 `Handle<T>` 为键；新增占位类型 `asset/public/type/mesh_asset.h`、`material_asset.h`。遗留：Handle 字段反射未接入（现有 `REG_FIELD` 宏不支持嵌套/复合类型，已记 TODO.md） |

**问题/风险**：~~两套资源标识体系并存~~ 已统一为 `Handle<T>`（断裂 #1 修复）。剩余缺口是 `OwnedHandle` RAII 与 channel-drop 延迟回收。

---

## 5.9 异步加载管线 ⭐

**知识库设计**：`AssetServer`（io_pool, event_queue）、加载状态机（Unloaded/Pending/Loading/Loaded/Ready/Failed/Unloading）、依赖 DAG（material→texture→mesh，topological reverse-propagation）、Bevy Gateway 模式（IO task pool + channel + ECS system 消费事件）、Branch C（IO 任务池 + Channel + ECS 网关）为默认推荐、Handle 立即返回 + 数据异步到达、循环依赖检测。

**代码现状** 🟡 部分完成（简化版单线程实现）

已实现（在 `_engine/source/asset/` 中）：
- `AssetServer`：单专用加载线程 + mutex 保护 pending task queue + completed callback queue、`loadSync`/`loadAsync`/`unload`/`reload`/`processEvents()`、`Handle<T>` 在分配时预留并在完成时填充 → `asset/public/loader/asset_server.h`、`asset/private/loader/asset_server.cpp`
- `IAssetLoader<T>` 接口 → `asset/public/loader/asset_loader.h`
- `TextureAssetLoader`：首个生产 loader，stb_image（Conan `stb/cci.20230920`）解码 → `TextureAsset`（RGBA8、左上原点）；失败返回空资产并记错误日志（2026-08-05，阶段 2b）→ `asset/public/loader/texture_asset_loader.h`、`asset/private/loader/texture_asset_loader.cpp`
- `MeshAssetLoader` + `.emesh` 二进制格式（魔数 "EMSH" + 版本 + 顶点/索引计数 + AABB + 原始顶点/索引 blob，小端无压缩；`writeMeshFile()` writer 供 cook 工具与测试共用）（2026-08-05，阶段 3a）→ `asset/public/type/mesh_format.h`、`asset/public/loader/mesh_asset_loader.h`、`asset/private/loader/mesh_asset_loader.cpp`
- `MeshCooker`（glTF → `.emesh` 离线 cook 工具，cgltf/Conan 解析，accessor 解码交错化为 `MeshVertex`，node 树烘焙世界变换输出 `scene.json`；Sponza 实跑：405 primitive 全量 cook 零告警，cooked 产物不入 git）（2026-08-05，阶段 3b）→ `_engine/tools/mesh_cooker/`
- `SceneLoader`（引擎侧，阶段 4c 自游戏侧迁入 `_engine/source/asset/scene/`，D5）：VFS 读 `scene.json`（core `JsonCursor` 固定 schema 解析，无 JSON 库）→ 每实体 `loadAsync` `.emesh` + 按 `.emat` 路径去重 `loadAsync` 材质 + spawn（烘焙 `GlobalTransform` + `MeshAssetRef` + 各自 `MaterialAssetRef` + `WorldAABB`）；VFS/AssetServer/loader/存储全部构造注入，引擎不持有游戏侧全局；`MaterialTextureBackfillSystem` 每帧回填 baseColor/normal/MR 贴图 Handle（4b/4c）。Sponza 实跑 405 实体 + 28 材质 + 73 贴图（25 baseColor + 24 normal + 24 MR）全量异步加载落地、无 fallback 残留（2026-08-05 阶段 3c 白模 / 2026-08-06 阶段 4b 贴图 / 2026-08-06 阶段 4c 迁引擎+normal/MR 落位）→ `asset/scene/public/scene_loader.h`、`asset/scene/private/scene_loader.cpp`
- VFS 存在（`vfs/public/vfs.h`、`mount_point.h`）——前置依赖满足
- ThreadPool 存在（`thread_pool/public/thread_pool.h`）——前置依赖满足

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| IO 任务池 | 笔记要求 IO task pool + work-stealing | ❌ 当前仅单线程 + mutex 队列（header 注释："Future upgrades: ThreadPool + work-stealing… lock-free MPSC channels"） |
| 依赖 DAG / LoadingGraph | 拓扑反向传播 + 依赖倒计数 | ❌ 不存在（header 注释："Phase 8+"） |
| 显式状态机 | Unloaded/Pending/Loading/Loaded/Ready/Failed/Unloading | ❌ 当前仅 pending→fill 简化流程 |
| ECS 集成 | AssetEvent<T> + ECS system 消费事件 | ❌ 未集成 |

---

## 5.10 资源热重载系统

**知识库设计**：开发期迭代效率、文件系统 Watch（跨平台抽象）、脏 Handle 标记、帧边界原地替换（`override_asset_handle`）、渲染安全性（延迟释放 / 帧边界同步 / Fence / 双缓冲）、导入管线（源资产 vs 引擎资产，Cook vs 按需编译+缓存）。

**代码现状** ❌ 未开始

搜索全局：`FileWatcher`、`hot.?reload`、热重载触发——**全部不存在**。`AssetServer::reload()`（同步 re-fill）存在但未绑定任何文件变更检测，没有帧边界替换逻辑，没有跨平台 Watch 抽象。

---

## 5.11 材质系统架构

**知识库设计**：材质 ≠ 着色器（多对多映射）、Template-Asset-Instance 三层分层（`ShaderTemplate` → `MaterialAsset` → `MaterialInstance`）、TAI 解耦"外观"与"GPU 代码"、variant 请求通过 keyword combo → permutation_id → async build technique、`TechniqueState`（Invalid→Pending→Valid + fallback）、按需异步编译 + 多级缓存（内存 → DDC/磁盘）。

**代码现状** 🟡 部分完成（仅 Phase 1 单层简化实现）

已实现：
- `Material` 类：Vertex/Fragment shader pair、CPU uniform block（std140 对齐）、按名参数 layout、PSO desc、`bind()` 上传 → `render/public/material/material.h/.cpp`
- `MaterialParamType` 枚举 + `MaterialParamDesc` → `render/public/material/material_types.h`
- `SimpleCubeRenderer` 示例：indexed cube + MVP shader 背靠 Material + GLRHIDevice → `render/public/material/example/simple_cube_renderer.h`
- `MaterialAsset`（asset 模块 CPU 侧，非 TAI 三层语义）：glTF pbrMetallicRoughness 字段（baseColorFactor[Vec3，弃 A]/metallic/roughness factor、normal/MR 贴图 Handle、`AlphaMode`/alpha_cutoff/double_sided）+ 贴图内容路径字符串（loader 只解析路径，Handle 由场景加载侧 loadAsync 回填）；`.emat` 由 mesh_cooker 每材质导出一个，`MaterialAssetLoader` 用 core JsonCursor 解析（2026-08-06 阶段 4a）→ `asset/public/type/material_asset.h`、`asset/private/loader/material_asset_loader.cpp`。阶段 4b：3c 临时 `shade_mode` 白模开关已退役（字段、shader `uShadeMode`/`uModel` 分支、共享 `mat_white` 全部拆除），baseColor 贴图 Handle 经 `MaterialTextureBackfillSystem` 轮询回填落位（2026-08-06）。阶段 4c：回填系统随场景加载迁入引擎 `asset/scene/`（`SceneLoader` 自持 `MaterialAssetLoader` 与场景材质清单，游戏侧 `RenderAssets` 的 Sponza 专属成员移除），normal/MR 贴图 Handle 经同一机制 loadAsync 落位（只加载不采样，D4，shader 消费端属阶段 5）

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| `ShaderTemplate` | categories + `GeneratePermutationHash` | ❌ 不存在 |
| `MaterialAsset` | template_ref + pass_hint + raster/depth state + parameters | ❌ 不存在 |
| `MaterialInstance` | asset_ref + technique_cache HashMap + `GetTechnique(keywords)` | ❌ 不存在 |
| `ShaderTechnique` | VS/PS/SRG/PSO bundle | ❌ 不存在 |
| `TechniqueState` | Invalid/Pending/Valid/Empty 状态机 | ❌ 不存在 |
| 三层 TAI 架构 | ShaderTemplate → MaterialAsset → MaterialInstance | ❌ 完全不存在。header 注释："Future extensions (Phase 2+)" |

---

## 5.12 着色器变体与编译缓存 ⭐

**知识库设计**：变体爆炸控制（2^N 组合灾难）、动态分支 vs 静态变体 vs 特化常量（各自适用场景）、按需异步编译 + fallback（pink/简化）、多级缓存（内存 → DDC/磁盘）、进程外编译预留（UE SCW 模式）、Material 关键字维度限制为 4-5 个。

**代码现状** 🟡 部分完成（离线编译工具链 + 内存级同步缓存）

已实现：
- `ShaderCache`：按 `(stage, sourceHash)` 去重（FNV-1a 哈希）、同步 `getOrCreateShader()`、仅内存缓存 → `render/private/material/shader_cache.h/.cpp`
- **离线着色器编译工具链**（6c，2026-08-07）：`_engine/tools/shader_compiler/` — HLSL SM 6.0 源码经 DXC 编译为 DXIL + SPIR-V，再经 SPIRV-Cross 交叉编译为 GLSL 330（UBO 展平为 plain uniform）。构建时自动运行（CMake post-build），产物输出到 `build/bin/{Config}/shaders/`。运行时通过 `Material::initFromBytecode()` 从磁盘加载预编译字节码。DXC 预编译包位于 `third_party/dxc/`（v1.9.2607），SPIRV-Cross 经 Conan 引入（`spirv-cross/1.4.350.0`）。同日修复：combined sampler 自动命名（`_<SPIR-V ID>`）按首次采样顺序分配而非 t0/t1/t2 声明顺序，曾导致 normal/MR 贴图绑反；现编译器在 `build_combined_image_samplers` 后把合并采样器重命名回原始 HLSL 贴图名（`uBaseColorTex`/`uNormalTex`/`uMRTex`），运行时按原名绑定；sky PS cbuffer 改名 `PerFramePS` 消除 VS/PS 展平 uniform 同名冲突。同日追加修复（截图机制辅助定位）：GLSL 输出目标从 330 升至 410 —— 330 下 FS 输入无法带显式 `layout(location)`，stage 接口按变量名链接，而 SPIRV-Cross 生成的 `out_var_*`/`in_var_*` 名恒不匹配，导致所有 FS 输入读零（贴图/法线/天空全失效）；410 后接口全带显式 location，截图验证恢复。

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| 变体系统 | permutation combos、keyword 维度限制 | ❌ 不存在 |
| DDC 磁盘缓存 | Derived Data Cache 文件系统持久化 | ❌ 不存在（header 注释："no DDC"） |
| 异步编译队列 | 后台线程 + 时间片预算 | ❌ 不存在。所有编译同步 |
| 进程外编译 | UE SCW 模式预留 | ❌ 不存在 |

---

## 5.13 材质参数绑定与 GPU 上传

**知识库设计**：每 Draw 独立上传的代价分析、Uniform Buffer 数组、`BindGroup`/`DescriptorSet` 池、Push Constants + Bindless（终极优化路径）、按更新频率分层（View/Light at binding 0-1、Material at binding 2、Object/Transform at binding 3）最大化 BindGroup 复用。

**代码现状** 🟡 部分完成（仅简单 uniform 立即模式）

已实现：
- CPU uniform block + per-draw `setUniform*` 系列（`setUniformFloat/Int/Vec2/3/4/Mat3/Mat4`）→ 通过 `IRHICommandList` 调用
- 光照/对象 uniform 链（阶段 5a，2026-08-06）：`RenderExecuteSystem::drawItem` 每 draw 下发 `uModel`/`uNormalMatrix`（core math 新增 `Mat3` 类型 + `normalMatrix` 逆转置 helper，`IRHICommandList`/`Material` 全链补 `Mat3`）/`uViewPos`/`uLightDir`/`uLightColor`/`uLightIntensity`/`uAmbient`；材质参数表扩至 13 项，`uMetallic`/`uRoughness` 自 `.emat` factor 传入。同日截图自验修正：glTF factor 是 MR 贴图的乘数（导出方有贴图时留 spec 默认 1.0），当常量用会把全部 25 个贴图材质判成全金属、漫反射归零（kD=(1-F)(1-metallic)=0，画面只剩高光+环境项、天花板纯色）；`prepareMaterial` 改为「有 MR 贴图 → 占位 metallic 0.0/roughness 0.9，factor-only 材质用真 factor」，5b 采样 MR 贴图后恢复 factor×texture。阶段 5b（2026-08-06）：参数表 13→17 项（新增 `uNormalTex`/`uMRTex`/`uHasNormalTex`/`uHasMRTex`），MR 占位已移除、恢复 factor×texture 语义（shader 内 `uHasMRTex` 分支采样 G=roughness/B=metallic 与 factor 相乘）
- OpenGL 后端：`glUniform*` + 缓存 `(program, name) → location` map → `gl_rhi_device.h` (line 148-167, 212-223)
- `Material::setTexture` 绑定纹理单元
- baseColor 贴图绑定链路（阶段 4b，2026-08-06）：`PrepareAssetsSystem::prepareMaterial` 把 `MaterialAsset::base_color_texture` prepare 成 RHI 纹理随材质下发（未就绪走 1x1 白纹理/粉色 fallback + 热替换），shader 采样 `uBaseColorTex`；normal/MR 贴图 Handle 已加载落位（阶段 4c），阶段 5b（2026-08-06）起经同一机制 prepare 下发并由 shader 采样（见 5.14 的 5b 条目）
- 调试统计面板数据链（阶段 5c，2026-08-06，D7）：`FrameStats` 扩展 `pso_cache_size`/`tracked_memory_bytes`/`gpu_total_bytes`/`gpu_available_bytes`，`RenderFrameRunner::runFrame` 每帧自 `IRHIDevice::queryMemoryInfo()`/`getTrackedMemoryUsage()` 与 Execute 的 PSO 缓存填充；`buildRenderStatsPanel` 改收 `RenderStatsParams` POD（FPS/draw calls/visible/culled/total/PSO cache/显存，ImGuiLib 保持零引擎依赖）。实跑校对：GPU Tracked 6.22 GiB，与 5b mip 链修复后的预期量级（约 6.2 GiB）吻合；`queryMemoryInfo()` 在无 NVX/ATI 扩展的机器上返回 0，面板显示 n/a

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| `BindGroupLayout` | 按材质类型定义 BindGroup 布局 | ❌ 不存在 |
| `MaterialBindGroupAllocator` | 按 TypeId 池化 BindGroup | ❌ 不存在 |
| `BindGroup`/`DescriptorSet` 池 | 数百对象共享少量 BindGroup | ❌ 不存在 |
| Push Constants | 高频参数走 push constant | ❌ 不存在 |
| Bindless Descriptor Heap | 大规模纹理数组 | ❌ 不存在 |
| 按更新频率分层 | View/Material/Object 三层 binding | ❌ 不存在 |
| UBO Buffer Object | GPU 端 Uniform Buffer | ❌ 当前是立即 `glUniform*` 调用，不是 UBO |

**问题/风险**：AGENTS.md 记录此为明确债务 `Render/UniformBinding`。当前方案在 10+ 材质时性能不可接受（每个 draw call 都做多次 `glUniform*` 系统调用）。

---

## 5.14 材质系统的 ECS 表达

**知识库设计**：材质内嵌 Mesh 组件的反模式、Handle 分离 + 按类型分 System、Extract → Prepare → Queue → PhaseSort → Render 完整数据流、AI 可观测性（ECS Component + GPU 自动同步 via `Changed<T>`）、`PreparedMaterial`（bind_group + pipeline_key）、Agent 组件白名单（Block 内部材质）。

**代码现状** 🟡 部分完成（Extract / Prepare / Queue / Render 已通，PreparedMaterial 仍是简化版）

已实现：
- Handle 分离：主 World 使用 `MeshAssetRef`/`MaterialAssetRef`（`Handle<MeshAsset>`/`Handle<MaterialAsset>`，2026-08-05 阶段 2a）→ `public/components/MeshAssetRef.h`、`MaterialAssetRef.h`
- CPU 侧资产类型：`MeshAsset`（交错顶点流 position/normal/uv/tangent + 索引 + AABB）与 `TextureAsset`（RGBA8 + 尺寸）→ `asset/public/type/mesh_asset.h`、`texture_asset.h`（2026-08-05，阶段 2b）
- `PrepareAssetsSystem`：扫描 Render World 的 `RenderMesh`/`RenderMaterial` Handle，经游戏侧 `Assets<T>` 解析并缓存 GPU 资源（`PreparedMesh`/`PreparedMaterial`/纹理 RHIRef）；未加载实体回退到单位立方体 + 品红材质，异步加载落地后自动替换；材质贴图未就绪则整体 pending（2026-08-05，阶段 2c）→ `prepare/PrepareAssetsSystem.h/.cpp`
- `ExtractRenderablesSystem`：拷贝 mesh/material refs → `RenderMesh`/`RenderMaterial`
- 组件注册：`REFLECT_COMPONENT` + `registerRenderComponents()` → `private/components/component_registration.cpp`
- `RenderExecuteSystem`：消费 `ViewBinnedPhases`/`ViewSortedPhases` 发出 GPU draw call（经 Prepare 查询 Prepared 资源 + fallback；unlit 材质 uMVP/uColor/uAlphaCutoff/uBaseColorTex——3c 的 uShadeMode/uModel 白模分支已随 4b `shade_mode` 退役拆除，2026-08-06）→ `execute/RenderExecuteSystem.h/.cpp`
- `.emat` 材质 cook 与解析：mesh_cooker 导出 28 个 `.emat`（贴图内容路径 + pbr factor + alphaMode/doubleSided），`scene.json` 实体 `material` 字段改为 `.emat` 清单相对路径；`MaterialAssetLoader` 只解析不触发贴图加载（D1）。Sponza 实数：0 mask / 1 blend / 2 doubleSided / 3 缺 baseColor 贴图（2026-08-06 阶段 4a）
- baseColor 贴图上屏（阶段 4b，2026-08-06）：scene_loader 按 `.emat` 路径去重 `loadAsync`、实体挂各自 `MaterialAssetRef`；`MaterialTextureBackfillSystem` 每帧回填 baseColor 贴图 Handle；Prepare 新增「path 非空 + Handle 无效 → pending」守卫与管线变体（`double_sided` → `CullMode::None`，`mask` → fs discard，blend 暂按 opaque 记 TODO）；cooker UV 写盘时 V 翻转（D2）。实跑：405 mesh + 25 贴图 + 28 材质全落地，零 pending 零 ERROR/WARN，`draw_calls == visible`、~60fps 无回归（`logs/game_run_4b.log`）；视觉验收待用户目视确认
- 方向光 + lit PBR（阶段 5a，2026-08-06）：主 World 新增 `DirectionalLight` 组件（direction/color/intensity/ambient）+ `ExtractLightSystem` → Render World `ExtractedLight`（取第一个，同相机模式）；`ExtractedView` 补 `view_pos`；fs 重写为 lit PBR——albedo（baseColor 贴图×uColor，pow 2.2 转线性）→ Lambert diffuse + GGX Cook-Torrance specular（D=GGX/TR、F=Schlick、G=Smith）+ 常量环境项 → pow(1/2.2) 输出，保留 `uAlphaCutoff` discard 与 doubleSided `gl_FrontFacing` 翻法线；游戏侧 `GamePlugin::setup()` spawn 一盏暖色日光，ImGui Debug 面板经 `DirectionalLightParams` POD 镜像运行时调节（ImGuiLib 保持零引擎依赖）。Debug 构建通过、测试 220 全绿（207 基线 + 8 Mat3 + 5 ExtractLight）、lint 239 文件零违规；实跑 35s 零 ERROR/WARN、启动期后零 pending/fallback 日志、`draw_calls == visible`、fps 56–61 无回归（`logs/game_run_5a.log`）；目视验收待用户确认
- 场景加载入口迁引擎 + normal/MR 落位（阶段 4c，2026-08-06）：scene_loader 自游戏侧迁入引擎新模块 `asset/scene/`（SceneLib，D5）——`SceneLoader` 构造注入 VFS/AssetServer/loader/存储（引擎不持有游戏侧全局），自持 `MaterialAssetLoader` 与场景材质清单；游戏侧 `GamePlugin::setup()` 场景加载剩一行调用，`RenderAssets` 的 Sponza 专属成员（`material_loader`/`scene_materials`）移除；`MaterialTextureBackfillSystem` 随迁为 `SceneLoader&` 薄封装。normal/MR 贴图与 baseColor 同一机制 `loadAsync` 落位 + Handle 回填（D4，Prepare/shader 不动）。实跑 30s：405 实体 + 73 贴图（25 baseColor + 24 normal + 24 MR）全落地零失败，零 pending/ERROR/WARN，帧统计无回归（`logs/game_run_4c.log`），与 4b 行为一致
- 法线贴图 + MR 贴图采样（阶段 5b，2026-08-06）：`prepareMaterial` 扩展——normal/MR 贴图复用 baseColor 的 pending 守卫/白纹理 fallback/热替换机制 prepare 下发；vs 消费顶点 TANGENT（loc 3 = vec4(tangent.xyz, tangentW)），N 走 `uNormalMatrix`、T 走 `mat3(uModel)` 后 Gram-Schmidt 正交化输出，fs 以 `B = cross(N,T) * tangentW` 重建 TBN（D5）、采样 tangent-space 法线贴图（tex*2-1）扰动 N，MR 贴图 G=roughness/B=metallic 与 factor 相乘；无贴图材质经 `uHasNormalTex`/`uHasMRTex` 手写分支走 factor-only。5a 的 MR 占位（metallic 0.0/roughness 0.9）移除。Debug 构建通过、测试 220 全绿、lint 239 文件零违规；实跑 73 贴图全部 prepare 上传（RGBA8 无 mipmap 合计约 4672 MiB GPU 纹理显存，留作 5c 显存面板校对基准）、28 材质 ready、零 ERROR/WARN、60 fps 无回归、`draw_calls == visible`（`logs/game_run_5b.log`）；截图验收砖缝/石块凹凸方向正确无反转、木门金属件与石材高光响应差异可见（`logs/shot_5b_default.png`）。同日复查截图发现全场高频颗粒噪点，根因为 `prepareTexture` 以 `mipLevels=1` 上传（GL 退化 `GL_LINEAR`，4K 贴图缩小采样 aliasing），已修复为按 `max(w,h)` 生成完整 mip 链（`glGenerateMipmap` + trilinear），噪点消除、60 fps 无回归（`logs/shot_5b_mip.png`）；纹理显存因此约 +33%（4672 MiB 基准作废，5c 面板预期约 6.2 GiB 量级）；目视验收待用户确认
- 天空渐变 pass + 天空 ECS 链（阶段 5c，2026-08-06，D6）：主 World 新增 `SkySettings`（zenith/horizon 颜色 + enabled）+ `ExtractSkySystem` → Render World `ExtractedSky`（取第一个，同相机/光照模式）；`RenderExecuteSystem` 自持天空 Material + 全屏三角形 VBO（3 顶点 NDC），clear 后、opaque 前绘制——vs 以 `uInvViewProj` 重建远平面世界位置、NDC z=1 钉在远平面（depthFunc LessEqual、depth write off），fs 按视线方向 y 分量在地平线色/天顶色间插值，输出走与 lit PBR 相同的 pow(1/2.2) 近似 gamma；游戏侧 `GamePlugin::setup()` spawn 默认日光渐变，ImGui Debug 面板经 `SkyParams` POD 镜像运行时调节（含 enabled 开关）；主 World 无 `SkySettings` 则跳过天空 pass、保留纯色 clear。天空 pass 是 view 级绘制，不计入 `draw_calls`（保持 draw_calls == visible 语义）。Debug 构建通过、测试 223 全绿（220 基线 + 3 ExtractSkySystem）、lint 244 文件零违规；实跑零 ERROR/WARN、73 贴图全落地、60–61 fps 无回归（`logs/game_run_5c.log`）；截图确认天空渐变为蓝色、统计面板数字合理（`logs/shot_5c_sky3.png`，GPU Tracked 6.22 GiB 与 5b 预期吻合、PSO Cache=3），天空观感用户已目视确认

**缺失项**：

| 项目 | 笔记要求 | 当前状态 |
|------|---------|---------|
| Prepare 阶段 | 异步解析 asset_id → GPU 几何/管线/材质参数 | ✅ `PrepareAssetsSystem`（2026-08-05，阶段 2c）：Extract→Prepare→Cull→Queue→Execute 链已通；fallback + 异步热替换生效。简化点：Prepare 不主动发起 loadAsync（Handle 无路径，由游戏侧发起加载）；材质创建仍同步编译 shader |
| `PreparedMaterial` | bind_group + pipeline_key 中间结构 | 🟡 简化版存在（`prepare/PreparedResources`，Material + 纹理引用，无 bind_group/pipeline_key） |
| `PipelineCache` Resource | ECS Resource 管理 PSO | ❌ 不存在 |
| Render 阶段 | 消费 PhaseItems 发出 GPU draw call | ✅ `RenderExecuteSystem`（2026-08-04，阶段1；单 view、单 cmdList、无 instancing） |
| AI `Changed<T>` 同步 | 修改组件 → 自动同步 GPU | ❌ 不存在 |
| `Handle<T>` 集成 | MeshAssetRef/MaterialAssetRef 应使用 asset 模块的 `Handle<T>` | ✅ 已集成（2026-08-05，阶段 2a；反射字段未接入，记 TODO.md） |

**问题/风险**：Prepare 断裂已修复（阶段 2c）。剩余：SortKey 仍按 `handle.index` 分箱而非 pipeline key；`PreparedMaterial` 无 bind_group/pipeline_key；shader 同步编译。

---

## 5.15 2D 渲染基础与批次合批

**知识库设计**：3D 管线为什么不能画 2D（投影语义、深度排序、光照/材质浪费）、ECS 中 2D 数据表达（Render World 独立 2D 层）、`SpriteBatch` 动态合批（vertex buffer 动态填充，每帧按 texture→material 排序合并）、纹理图集（Atlas 预打包）、GPU Instancing 预留、ImGui 协作边界。

**代码现状** ❌ 未开始

搜索全局：`Sprite`、`SpriteBatch`、`Sprite2D`、纹理图集、2D 渲染——**全部不存在**。`RenderPhase::UI` 枚举值存在（queue/PhaseItem.h）但仅作为空的 phase bucket。

---

## 5.16 字体渲染系统

**知识库设计**：位图字体缩放困境、单通道 SDF 原理（锐角变圆）、MSDF（RGB 三通道保留锐角，行业标准）、字形图集生成（`stb_truetype`/`msdfgen`/`msdf-atlas-gen`）、文本布局基础（字形放置、行高、对齐）、极小字号位图降级路径。

**代码现状** ❌ 未开始

搜索全局：`Font`、`SDF`、`MSDF`、`glyph`、`stb_truetype`——**全部不存在**。工程中有 `glad`（OpenGL loader）但没有字体相关库。

---

## 5.17 UI 画布与场景叠加

**知识库设计**：Screen / World / Viewport 三种画布空间、多画布系统、RenderGraph 节点插入、后处理与 UI 隔离、阶段 5 自研 2D 与阶段 6 自研 UI 框架的边界。

**代码现状** ❌ 未开始

搜索全局：`Canvas`、`ScreenCanvas`、`WorldCanvas`、UI 画布——**全部不存在**。`ImGui` 集成是第三方库接入（`imgui_manager.cpp`），不是引擎自研 UI 画布系统。

---

## 5.18 后处理栈架构

**知识库设计**：统一 `SceneTextures`（SceneColor HDR、SceneDepth、velocity、customDepth）、RenderGraph/DAG Pass 编排（自动拓扑排序、死 Pass 剔除、瞬态别名）、全屏 Pass 执行模型（PS triangle / Compute / Uber）、时序效果历史帧池化管理（pooled extract/register）、性能预算与动态质量、AI 友好 ECS 接口（`PostProcessStack` 作为相机 ECS Component + AI Schema）。

**代码现状** ❌ 未开始

搜索全局：`PostProcess`、`SceneTextures`、后处理栈——**全部不存在**。

注意：知识库中 5.18 和 5.19 笔记已完成撰写（✅），RoadMap 的模块状态速查也标记为已完成，但代码实现为零。

---

## 5.19 后处理效果算法

**知识库设计**：ToneMapping（Reinhard / Filmic / ACES / AGX + TonyMcMapface）、Bloom（prefilter + downsample chain → upsample + blend、Karis firefly 抑制、能量守恒 vs 叠加）、TAA（velocity + reprojection + YCoCg AABB clamp）、SSAO（classic / HBAO+ / GTAO）、MotionBlur（TAA 之前执行）。

**代码现状** ❌ 未开始

搜索全局：`Bloom`、`Tonemap`、`ToneMap`、`TAA`、`SSAO`、`MotionBlur`——**全部不存在**。

---

## 5.20 基础粒子系统

**知识库设计**：粒子对 ECS 的特殊挑战（高 churn 破坏 Archetype）、CPU vs GPU vs 表达式驱动、SoA 连续数组存储（非每粒子 Entity）、与透明渲染 + Bloom 的交互（粒子发射 → Transparent Phase → Bloom 捕捉自发光）、发射器组件设计、GPU 升级路径预留。

**代码现状** ❌ 未开始

搜索全局：`Particle`、`ParticleEmitter`、`ParticleSystem`、SoA 粒子存储——**全部不存在**。

---

## 前置依赖模块状态

部分 Render 模块依赖以下非 Render 模块，需要确认它们的状态：

| 依赖模块 | 所属阶段 | 状态 | 说明 |
|---------|---------|------|------|
| 资源句柄系统 (`Handle<T>`) | 5.8 | ✅ 存在于 `asset/` 模块 | 已与 Render 组件集成（2026-08-05，阶段 2a） |
| 文件 IO / VFS | 3.5 | ✅ 存在 | `vfs/` 模块：VFS + mount、`readFile`/`writeFile`、`FileData`、`IMountPoint` |
| 线程池 | 3.6 | ✅ 存在 | `thread_pool/` 模块：Chase-Lev work-stealing deques、`parallelFor`、overflow queue。已被 Cull 和 Queue 系统使用 |
| 数学基础 | 3.2 | ✅ 存在 | Vec3/Mat4/Quat/AABB/Frustum |
| 容器系统 | 3.4 | ✅ 存在 | Array/HashMap/SparseSet/RingBuffer |
| 反射系统 | 4.4 | ✅ 存在 | `REFLECT_COMPONENT` 宏 + 组件注册 |

---

## 总体进度汇总

```
5.1  RHI 抽象层与命令模型           ██████████  98%  多后端接口对齐(6a)+延迟命令缓冲(6b)+D3D12 后端(6d)完成，GL/D3D12 双后端等价；UBO 绑定待 6e
5.2  多线程命令录制与并行渲染 ⭐      ██████░░░░  60%  CPU 并行生成完成，GPU 命令并行录制未实现
5.3  GPU 资源生命周期管理 ⭐          ████████░░  80%  核心机制齐全，缺少预算强制和 ECS 化
5.3b PSO 缓存与异步编译 ⭐            ████░░░░░░  40%  同步缓存已接线（5c，28 材质收敛为 3 PSO），无异步编译
5.4  ECS 架构下的渲染世界设计 ⭐      ████████░░  80%  双 World + Extract + 帧驱动层完成，缺少 Prepare 阶段
5.5  可见性判断与空间加速结构 ⭐      ███░░░░░░░  30%  仅基础视锥剔除，无 BVH/HZB/GPU-Driven
5.6  渲染队列与 DrawCall 组织 ⭐      ████████░░  80%  SortKey/Binned/Sorted/Queue 完整，Instancing 预留
5.7  RenderGraph 与多 Pass 资源管理 ⭐ ░░░░░░░░░░   0%  完全未开始
5.8  资源句柄与引用计数              █████████░  90%  asset 模块完整且已集成 Render 组件（2a），缺 OwnedHandle/延迟回收
5.9  异步加载管线 ⭐                  ████░░░░░░  40%  简化单线程实现 + 生产 loader×2（stb_image 纹理 2b、.emesh mesh 3a）+ glTF cook 工具（3b），无 IO pool/DAG/状态机
5.10 资源热重载系统                   ░░░░░░░░░░   0%  完全未开始
5.11 材质系统架构                     ███░░░░░░░  25%  仅 Phase 1 单层简化，无 TAI 三层
5.12 着色器变体与编译缓存 ⭐          ██░░░░░░░░  20%  仅内存级同步 ShaderCache
5.13 材质参数绑定与 GPU 上传          ██░░░░░░░░  20%  仅立即 uniform 调用 + 统计面板数据链（5c），无 BindGroup/PushConstants
5.14 材质系统的 ECS 表达              ███████░░░  70%  Extract/Prepare/Queue/Render 全通（2c），PreparedMaterial 简化版
5.15 2D 渲染基础与批次合批            ░░░░░░░░░░   0%  完全未开始
5.16 字体渲染系统                     ░░░░░░░░░░   0%  完全未开始
5.17 UI 画布与场景叠加                ░░░░░░░░░░   0%  完全未开始
5.18 后处理栈架构                     ░░░░░░░░░░   0%  笔记已完成，代码为零
5.19 后处理效果算法                   ░░░░░░░░░░   0%  笔记已完成，代码为零
5.20 基础粒子系统                     ░░░░░░░░░░   0%  完全未开始
```

---

## 关键集成断裂（Blocking Issues）

### 断裂 #1: `asset/` Handle 系统与 Render 组件不互通 ✅ 已修复（2026-08-05，阶段 2a）
`asset/` 模块有完整的 `Handle<T>` + `HandleTable<T>` + `Assets<T>`，但 `MeshAssetRef`/`MaterialAssetRef` 使用裸 `u32 asset_id`。两套资源标识体系并存，互不通信。
→ 全部渲染组件迁移为 `Handle<MeshAsset>`/`Handle<MaterialAsset>`；新增占位类型 `MeshAsset`/`MaterialAsset`（asset 模块，字段留待 2b 填充）；`RenderExecuteSystem` 注册表键改为 `Handle<T>`；游戏侧 ID 常量改为 `render_assets.h` 中经 `Assets<T>::insert` 获得的 Handle。SortKey 仍取 `handle.index & 0xFFFF`（行为不变）。遗留：Handle 字段反射未接入（TODO.md）。

### 断裂 #2: Prepare 阶段缺失——Extract→Cull→Queue 链断裂 ✅ 已修复（2026-08-05，阶段 2c）
`ExtractRenderablesSystem` 将 `(MeshAssetRef, MaterialAssetRef)` 拷贝进 Render World 为 `(RenderMesh, RenderMaterial)`，但没有任何 Prepare 阶段将 Handle 解析为 GPU 几何/管线/材质参数。`QueueDrawsSystem` 用 `handle.index & 0xFFFF` 做粗略的分箱，但没有真正的管线状态对象。
→ 阶段 2b 补齐 CPU 侧前置（`MeshAsset`/`TextureAsset`/`TextureAssetLoader`）；阶段 2c 新增 `PrepareAssetsSystem`（`render_system/prepare/`）：缓存 handle→`PreparedMesh`/`PreparedMaterial`/纹理 RHIRef，未加载回退单位立方体 + 品红材质，异步落地后自动替换；`RenderExecuteSystem` 删除手工注册入口改为消费 Prepared 资源；`main.cpp.in` 手工注册块删除，帧链变为 Extract→Prepare→Cull→Queue→Execute。偏差：Prepare 不主动发起 loadAsync（Handle 无路径），加载由游戏侧发起。

### 断裂 #3: 没有渲染消费端 ✅ 已修复（2026-08-04，阶段1）
`QueueDrawsSystem` 产出了 `ViewBinnedPhases`/`ViewSortedPhases`，但没有 System 消费这些 Phase 容器发出实际的 GPU draw call。
→ 已新增 `RenderExecuteSystem`（`render_system/execute/`）：遍历四个 Phase 容器，逐 `PhaseItem` 解析网格/材质注册表并发出 `drawIndexed`。

### 断裂 #4: 没有帧驱动层 ✅ 已修复（2026-08-04，阶段1）
Extract/Cull/Queue 各 System 通过单元测试独立验证，但没有 `RenderSystem::runFrame()` 生产级帧循环串联全流程。
→ 已新增 `RenderFrameRunner`（`render_system/frame/`）：`runFrame(mainWorld, dt)` 每帧串联 Extract → Cull → Queue → Execute，主循环（`main.cpp.in`）已切换到此路径，硬编码 `SimpleCubeRenderer` 绘制已移除（类保留）。

---

## 建议的下一步优先级

基于依赖关系和当前进度，建议实现顺序：

1. **5.14 + 5.8 集成** — 将 `asset/` 的 `Handle<T>` 集成到 Render 组件中（修复断裂 #1），补齐 Prepare 阶段（修复断裂 #2）
2. **5.7 RenderGraph 最小实现** — RenderGraph 编译器 <500 行即可打通多 Pass 依赖管理，是 5.18/5.19 的前置
3. ~~**5.1 命令缓冲**~~ — ✅ 已完成（6b，2026-08-07）
4. **5.11 材质系统升级** — 从 Phase 1 升级到 TAI 三层（当前 Phase 1 的 Simplistic Material 是明确的临时代码）
5. **5.15 2D 渲染** — 解锁 5.16 字体和 5.17 UI 画布，为阶段 6 自研 UI 框架提供基础
