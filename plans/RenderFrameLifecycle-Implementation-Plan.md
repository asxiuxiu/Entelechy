# 渲染一帧的生命周期 — Vibe Coding 实现计划

> **参考笔记**: `Notes/SelfGameEngine/渲染管线与第一帧/渲染一帧的生命周期`
> **前置依赖**: 资源管理、场景图与变换、RHI 抽象层、材质与着色器系统
> **核心问题**: 在 ECS 架构下，一帧画面如何从游戏世界状态逐步转化为 GPU 命令并最终呈现在屏幕上？

---

## 1. 架构大图：数据流的 6 层转换

```
主 World 状态
    │ Extract（快照）
    ▼
Render World 组件
    │ Prepare（Handle → GPU 资源）
    ▼
可见实体列表
    │ Culling（视锥测试）
    ▼
按 Phase 组织的 Draw Items
    │ Queue（排序/分箱）
    ▼
声明式 Pass 图
    │ RenderGraph.Compile()（拓扑排序 + Barrier 推导）
    ▼
GPU 命令缓冲
    │ Execute + Submit + Present
    ▼
屏幕像素
```

**每一层的产出都是显式的 ECS Resource，可被快照、被调试、被 AI 观测。**

---

## 2. 渐进路径：5 个 Vibe Checkpoints

不是按时间排期，而是按"能不能画出来"的验证节点组织。每完成一个 Checkpoint，你应该能运行一个 demo 并看到正确输出。

---

### Checkpoint 1: 双 World 建立 + Extract 闭环

**目标**: 主 World 有实体，Extract 后 Render World 也有对应的实体，能遍历 Render World 直接画出来。

**新增文件**:
```
render/
  RenderWorld.h/cpp              — Render World 容器与 Schedule 链
  ExtractSchedule.h/cpp          — Extract 阶段的 SystemSet
  ExtractRenderablesSystem.h/cpp — 搬运 (MeshHandle, MaterialHandle, GlobalTransform)
  ExtractCameraSystem.h/cpp      — 搬运 Camera → ExtractedView
  RenderComponents.h             — RenderMesh, RenderMaterial, RenderTransform
  RenderResources.h              — ExtractedView, MainWorldSync
```

**核心数据结构**:
```cpp
struct RenderMesh      { AssetId mesh_handle; };
struct RenderMaterial  { AssetId material_handle; };
struct RenderTransform { Mat4 world_matrix; };

struct ExtractedView {
    Mat4 view_matrix, proj_matrix;
    Frustum frustum;
    Rect viewport;
};

struct MainWorldSync {
    HashMap<Entity, Entity> main_to_render;  // 双向映射
    HashMap<Entity, Entity> render_to_main;
};
```

**关键设计意图**:
- Extract **只读**主 World，绝不修改。这是未来并行的安全边界。
- Extract 阶段**只复制 Handle**，不解析为 GPU 资源。避免阻塞在异步加载上。
- `MainWorldSync` 是 AI 可观测的——AI 能问"主 World 实体 E 对应渲染端哪个实体？"

**Vibe 验证**: 主 World 摆 100 个立方体，Extract 后 Render World 里也有 100 个实体，能串行遍历画出来。此时 Render World 仍跑在主线程上，但数据结构已预留并行空间。

---

### Checkpoint 2: 视锥剔除 + Phase 队列

**目标**: 相机背后的物体不画，Opaque 物体按材质分箱，Transparent 物体按深度排序。

**新增文件**:
```
render/
  Frustum.h/cpp                  — 视锥体 + AABB 相交测试
  FrustumCullSystem.h/cpp        — 逐实体视锥剔除
  ViewVisibleList.h              — 可见实体列表 Resource
  PhaseItem.h                    — 渲染阶段最小单元
  BinnedRenderPhase.h/cpp        — Opaque/AlphaMask 分箱
  SortedRenderPhase.h/cpp        — Transparent 深度排序
  QueueDrawsSystem.h/cpp         — 按 Phase 生成 Items
```

**核心数据结构**:
```cpp
struct ViewVisibleList {
    Array<Entity> entities;
};

enum class RenderPhase : uint8_t {
    ShadowMap = 0, Opaque3D = 1, AlphaMask = 2, Transparent3D = 3, UI = 4
};

union SortKey {
    uint64_t value;
    struct { uint32_t depth; uint16_t material_id; uint8_t phase; uint8_t reserved; };
};

struct PhaseItem {
    SortKey sort_key;
    Entity render_entity;
    uint32_t instance_count = 1;  // 预留 Instancing
};

class BinnedRenderPhase {
    HashMap<uint32_t, Array<PhaseItem>> bins;
public:
    void AddItem(PhaseItem item);
    void Clear();
    const auto& GetBins() const { return bins; }
};

class SortedRenderPhase {
    Array<PhaseItem> items;
public:
    void AddItem(PhaseItem item);
    void Prepare();  // 按 depth 排序
    void Clear();
    const auto& GetItems() const { return items; }
};
```

**关键设计意图**:
- 没有 `Aabb` 组件的实体默认"总是可见"（如天空盒）。
- `ViewVisibleList` 是独立 Resource——今天用 CPU 暴力剔除，明天换 BVH，后天换 GPU Compute，下游无感知。
- 排序键 64 位：高 8 位 phase，中 16 位 material，低 32 位 depth。不透明 depth 正序（Early-Z），透明 depth 逆序。
- **Instancing 接口预留**：`instance_count` 暂为 1，后续在 Bin 内自动合并。

**Vibe 验证**: 相机转动时，背后的物体从 `ViewVisibleList` 中消失；相同材质的立方体落在同一个 Bin 里；半透明物体从远到近绘制。

---

### Checkpoint 3: 简化版 Render Graph

**目标**: 不再手写 Barrier，用声明式 Pass + 自动推导管理一帧内的多 Pass 资源依赖。

**新增文件**:
```
render/
  RenderGraph.h/cpp              — 核心图结构
  RenderGraphResource.h          — RGTextureDesc, RGAccess 枚举
  RenderGraphBuilder.h/cpp       — Pass 声明时的 Builder
  BuildRenderGraphSystem.h/cpp   — 根据 Phase Items 填充 Pass
  CompileRenderGraphSystem.h/cpp — 拓扑排序 + Culling + 生命周期扫描
```

**核心数据结构**:
```cpp
class RenderGraph {
public:
    struct Pass {
        StringID name;
        Array<ResourceHandle> reads, writes;
        Function<void(ICommandContext*)> execute;
        bool culled = false, reachable = false;
    };
    struct Resource {
        StringID name;
        RGTextureDesc desc;
        bool imported = false;   // BackBuffer 等外部资源
        bool transient = true;   // 帧内临时资源，未来可别名
        Pass *first_writer = nullptr, *last_reader = nullptr;
    };

    TextureHandle CreateTexture(const char* name, const RGTextureDesc& desc);
    TextureHandle ImportTexture(const char* name, TextureHandle external);
    void AddPass(const char* name, Function<void(RGPassBuilder&)> setup);
    void Compile();   // 拓扑排序 + 死 Pass Culling + 生命周期扫描
    void Execute(ICommandContext* ctx);  // 自动插入 Barrier + 执行
    void Clear();
};

class RGPassBuilder {
public:
    void Read(TextureHandle handle);
    void WriteColor(uint32_t slot, TextureHandle handle);
    void WriteDepth(TextureHandle handle);
    void Execute(Function<void(ICommandContext*)> callback);
};
```

**关键设计意图**:
- **简化版定义**：拓扑排序 + Culling + 自动 Barrier 必须有；瞬态资源别名 + Async Compute 先不做。
- **死 Pass 自动剔除**：某个 Debug Pass 没连到 BackBuffer？`Compile()` 后自动 `culled = true`，不分配资源不录命令。
- **Imported Resource**：SwapChain BackBuffer 用 `ImportTexture`，不会被 Cull，生命周期由外部管理。
- **与 ECS 映射**：`RenderGraph` 是 Render World 的 Resource。`BuildRenderGraphSystem` 填 Pass，`CompileRenderGraphSystem` 调用 `Compile()`。

**最小管线示例**:
```cpp
// Shadow Pass → GBuffer Pass → Lighting Pass → PostProcess → BackBuffer
graph.AddPass("Shadow", [&](auto& b) {
    b.WriteDepth(shadow_map);
    b.Execute([=](auto* ctx) { DrawPhaseItems(ctx, shadow_phase); });
});
graph.AddPass("GBuffer", [&](auto& b) {
    b.WriteColor(0, gbuffer);
    b.WriteDepth(depth_buffer);
    b.Execute([=](auto* ctx) { DrawPhaseItems(ctx, opaque_phase); });
});
graph.AddPass("Lighting", [&](auto& b) {
    b.Read(shadow_map); b.Read(gbuffer); b.Read(depth_buffer);
    b.WriteColor(0, lit_scene);
    b.Execute([=](auto* ctx) { DrawFullscreenQuad(ctx); });
});
// ...
```

**Vibe 验证**: 新增一个未连接到 BackBuffer 的 Debug Pass，运行后确认它被 Cull（RG Dump 中不可见）；Lighting Pass 读取 ShadowMap 时 Barrier 被自动插入，画面无随机噪点。

---

### Checkpoint 4: 命令录制 → 提交 → 呈现 → 同步

**目标**: Render Graph 执行后，命令真正到达 GPU，画面出现在屏幕上，资源安全回收。

**新增文件**:
```
render/
  RecordCommandsSystem.h/cpp     — 遍历 RG Pass 在 ICommandContext 上录命令
  SubmitSystem.h/cpp             — Submit CommandBuffer
  PresentSystem.h/cpp            — SwapChain Present
  FrameFenceSystem.h/cpp         — Fence 等待 + 延迟删除
```

**核心接口**:
```cpp
class ICommandContext {
public:
    virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
    virtual void EndRenderPass() = 0;
    virtual void BindPipeline(PipelineHandle pso) = 0;
    virtual void BindVertexBuffer(GpuBuffer vb, uint32_t slot = 0) = 0;
    virtual void BindIndexBuffer(GpuBuffer ib) = 0;
    virtual void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1) = 0;
    virtual void ResourceBarrier(TextureHandle tex, ResourceState new_state) = 0;
};

class RenderDevice {
public:
    virtual ICommandContext* CreateCommandContext() = 0;
    virtual void Submit(ICommandContext* ctx) = 0;
    virtual void Submit(Array<ICommandContext*> ctxs) = 0;  // 预留多上下文并行
    virtual void Present(bool vsync) = 0;
    virtual void WaitForFrameFence(uint64_t frame_index) = 0;
    virtual void ProcessDelayedDeletes() = 0;
};
```

**关键设计意图**:
- **单线程初期**：`RenderWorldSystem` 串行执行 Compile → Execute → Submit → Present → WaitForFence。
- **RHI 线程预留**：`ICommandContext` 的接口是"命令式"的（追加到内部缓冲），不是直接调 D3D12。未来拆出 RHI 线程时，上层代码一字不改。
- **Fence 与延迟删除**：每帧 Submit 时插入 Fence 信号。资源标记 `"使用于第 N 帧"`，仅当 GPU 完成第 N 帧时才真正释放。这是避免 GPU 还在读、CPU 已经删的关键。
- **SwapChain 三重缓冲**：`Present()` 后 CPU 继续下一帧，GPU 后台执行。`WaitForFrameFence` 防止 CPU 超前 GPU 超过 1~2 帧。
- **VSync 开关**：`Present(true)` 用于正常游戏，`Present(false)` 用于性能分析观察无上限帧率。

**Vibe 验证**: 稳定运行不闪烁；关闭 VSync 后帧率突破显示器刷新率；通过计数器验证 GPU 完成后延迟删除资源被回收；窗口 Resize 后 BackBuffer 正确重建。

---

### Checkpoint 5: 并行化（可选，架构预留）

**目标**: 主线程与渲染线程并行，延迟一帧，但接口不变。

**修改点**:
- Extract 完成后主线程立即进入下一帧逻辑，不等待 Render World。
- Render World 在独立线程运行 Stage 1~4。
- RHI 线程（另一个独立线程）运行 Stage 5，通过线程安全队列接收命令缓冲。
- 双缓冲 Extract 数据，避免读写竞争。

**并行时间线**:
```
CPU 主线程:   [Main N] → [Extract N] → [Main N+1] → [Extract N+1] → ...
                    ↓           ↓
CPU 渲染线程:        [Render N]  →  [Render N+1]  → ...
                          ↓
GPU:                      [Execute N] → [Present N] → ...
```

**关键设计意图**:
- Checkpoint 1~4 的串行实现已经把这个并行的所有数据结构都准备好了。并行化只是**调度变更**，不是结构重写。
- `ICommandContext` 的命令缓冲语义让它天然适合跨线程搬运。
- 输入延迟 = 一帧（约 16ms @ 60fps），可接受。

**Vibe 验证**: ThreadSanitizer 无报警；CPU 多核利用率提升；画面无撕裂、无闪烁。

---

## 3. 数据流速查表

| 阶段 | 输入（ECS Resource / Component） | 处理 | 输出（ECS Resource） |
|------|----------------------------------|------|----------------------|
| Extract | `MeshHandle`, `MaterialHandle`, `GlobalTransform`, `Camera` | 只读拷贝到 Render World | `RenderMesh`, `RenderMaterial`, `RenderTransform`, `ExtractedView`, `MainWorldSync` |
| Prepare | `RenderMesh.handle`, `RenderMaterial.handle` | Handle → GpuBuffer / PipelineKey | 组件原地更新（解析后） |
| Culling | `RenderTransform`, `Aabb` (可选), `ExtractedView` | AABB-Frustum 相交测试 | `ViewVisibleList` |
| Queue | `ViewVisibleList`, `RenderMesh`, `RenderMaterial` | 生成 SortKey → Bin / Sort | `ViewBinnedPhases`, `ViewSortedPhases` |
| Build RG | Phase Items + 管线配置 | 声明 Pass 与资源读写 | `RenderGraph` (Resource) |
| Compile RG | `RenderGraph` | 拓扑排序 + Culling + Barrier 推导 | 已编译的 `RenderGraph` |
| Execute | 已编译 `RenderGraph` | 遍历 Pass → 录命令 | `ICommandContext` (命令缓冲) |
| Submit | `ICommandContext` | Submit → GPU 队列 | GPU 开始执行 |
| Present | SwapChain BackBuffer | Flip → 显示器 | 像素 |
| Fence | Frame Fence | 等待 GPU 完成 | `completed_frame` + 延迟删除回收 |

---

## 4. 目录结构（render 模块）

```
_engine/source/render/
├── AGENTS.md
├── CMakeLists.txt
└── source/
    ├── render_world/
    │   ├── RenderWorld.h/cpp
    │   └── ExtractSchedule.h/cpp
    ├── extract/
    │   ├── ExtractRenderablesSystem.h/cpp
    │   ├── ExtractCameraSystem.h/cpp
    │   └── MainWorldSync.h
    ├── components/
    │   ├── RenderComponents.h       # RenderMesh, RenderMaterial, RenderTransform
    │   └── RenderCamera.h           # ExtractedView
    ├── culling/
    │   ├── Frustum.h/cpp
    │   ├── FrustumCullSystem.h/cpp
    │   └── ViewVisibleList.h
    ├── queue/
    │   ├── PhaseItem.h
    │   ├── BinnedRenderPhase.h/cpp
    │   ├── SortedRenderPhase.h/cpp
    │   └── QueueDrawsSystem.h/cpp
    ├── render_graph/
    │   ├── RenderGraph.h/cpp
    │   ├── RenderGraphResource.h
    │   ├── RenderGraphBuilder.h/cpp
    │   ├── BuildRenderGraphSystem.h/cpp
    │   └── CompileRenderGraphSystem.h/cpp
    ├── execute/
    │   ├── RecordCommandsSystem.h/cpp
    │   ├── SubmitSystem.h/cpp
    │   ├── PresentSystem.h/cpp
    │   └── FrameFenceSystem.h/cpp
    └── RenderResources.h            # ViewBinnedPhases, ViewSortedPhases
```

---

## 5. 外部接口假设（RHI 层已提供）

```cpp
// 这些由 RHI 抽象层实现，本计划只消费、不实现
class ICommandContext;
class RenderDevice;
using TextureHandle = /* RHI 纹理句柄 */;
using GpuBuffer     = /* RHI 缓冲区句柄 */;
using PipelineHandle = /* RHI 管线句柄 */;

enum class ResourceState { RenderTarget, ShaderResource, DepthWrite, /* ... */ };
```

**如果 RHI 层接口与上述不符，优先通过适配层桥接，不改动本计划的架构设计。**

---

## 6. 关键决策回顾

| 决策 | 选择 | 理由 |
|------|------|------|
| 逻辑-渲染并行 | 双 World + Extract | Bevy 的 ECS-native 方案，零锁并行，AI 可观测 Render World 完整状态 |
| 状态双缓冲 | 不做完整双缓冲 | Bevy 的 Extract 只做增量拷贝，UE 用 Proxy 模式。完整双缓冲内存开销不值得 |
| 数据提取 | Extract + Render World + Proxy 数组 | Extract 解决安全并行，Phase 内部连续数组解决 SIMD/缓存友好 |
| 可见性剔除 | 初期逐实体视锥测试 | Bevy 的 CPU 剔除是 ECS 最自然起点。<5000 实体完全够用 |
| Draw Call 组织 | Binned Phase + Sorted Phase | Bevy 0.19 的 Binned/Sorted 双策略，不透明分箱、透明排序 |
| 资源依赖管理 | 简化版 Render Graph | UE RDG 证明了这是管理多 Pass 的最小正确抽象。简化版只保留拓扑排序+Culling+Barrier |
| 命令提交 | 单线程初期，预留 RHI 线程 | UE 三线程模型是黄金标准，但初期合并渲染+RHI 线程可降低调试复杂度 |

---

## 7. Vibe 时的调试链路

画面出问题？沿着这个链条逐层检查：

1. **Extract 阶段** → Render World 里有没有这个实体？`MainWorldSync` 映射对吗？
2. **Culling 阶段** → `ViewVisibleList` 里有没有它？AABB 是不是 `(0,0,0)`？Frustum 是不是反了？
3. **Queue 阶段** → 它进了正确的 Phase 吗？Opaque 进了 Binned、Transparent 进了 Sorted？
4. **RenderGraph 阶段** → 它的 Pass 被意外 Cull 了吗？`DumpGraphviz()` 看看。
5. **Execute 阶段** → 命令写入 `ICommandContext` 了吗？DrawCall 计数器有增加吗？
6. **Submit/Fence 阶段** → GPU Fence 到达了吗？延迟删除的资源是不是把 Buffer 提前释放了？

**每一层的产出都是显式 Resource，打断点就能看。**
