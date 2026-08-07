# Render 模块
> 路径：`_engine/source/render`

## 一句话职责

图形渲染后端抽象与多后端实现（OpenGL + D3D12），包含 RHI（Render Hardware Interface）抽象层与延迟命令缓冲。

## 关键文件
| 文件 | 职责 |
|------|------|
| `rhi_types.h` | RHI 基础类型：Buffer/Texture/Shader 枚举、资源描述结构、渲染通道描述、`RHIFenceValue`、`RHIMemoryInfo` |
| `rhi_resources.h` | GPUResource 基类（引用计数 + 延迟删除支持）、RHIRef 智能句柄、具体资源类型声明 |
| `rhi_device.h` | `IRHIDevice`（资源工厂 + 提交 + Frame Fence + 延迟删除 + 显存预算）与 `IRHICommandList`（命令录制 + 调试标注）纯虚接口 |
| `rhi_pipeline.h` | `PipelineStateDesc`（完整 PSO 描述，含哈希支持）、`PSOManager`（设备级缓存；2026-08-06 阶段 5c 起 `GLRHIDevice::createPipelineState` 经 `find`/`insert` 走缓存，相同 shader pair + 状态共享一个 GL program） |
| `rhi_transient_resource_pool.h` / `.cpp` | 瞬态纹理池：按描述符分组复用单帧生命周期纹理，预留显存别名接口 |
| `gl_rhi_device.h` / `.cpp` | OpenGL 后端对 RHI 接口的实现：`GLRHIDevice`、GL 资源对象、Fence 跟踪、延迟删除队列、显存统计；经 pimpl `DeferredState` 持有命令缓冲三件套（6b 起录制+回放） |
| `render_command_buffer.h` / `.cpp` | 延迟命令缓冲（6b）：连续内存 bump allocator + `RenderCommandType` 24 种命令 payload |
| `deferred_command_list.h` / `.cpp` | `IRHICommandList` 的纯序列化实现（录制进 `RenderCommandBuffer`，零图形 API 调用） |
| `gl_command_translator.h` / `.cpp` | GL 翻译器：遍历命令缓冲逐条翻译为 GL 调用（状态缓存从旧 `GLCommandList` 迁入） |
| `d3d12_rhi_device.h` / `.cpp` | **D3D12 后端（6d）**：`D3D12RHIDevice`（swapchain/深度/2 帧 in-flight/upload ring/SRV heaps/显存查询/readback）、资源对象、`D3D12Fence`；cbuffer 布局经 DXC 容器反射（`IDxcContainerReflection`，需 dxcompiler.dll+dxil.dll） |
| `d3d12_command_translator.h` / `.cpp` | D3D12 翻译器：全局 root signature（CBV b0-b2 按 stage 拆 slot + SRV 表 + 3 静态 sampler）；`setUniform*` 展平名解析回 cbuffer staging，draw 时上传 upload ring 绑 root CBV |
| `rhi_device_factory.h` / `.cpp` | `createRHIDevice()` 工厂 + `shaderFileExtensionForBackend()`/`shaderFormatForBackend()`（按后端选 `.glsl`/`.dxil`/`.spv`） |
| `rhi_resources.cpp` | `GPUResource::release()` 实现：引用计数归零后交给所属设备延迟删除 |
| `opengl_backend.cpp` | OpenGL 初始化、视口管理、清除与呈现（SwapChain 层，保留兼容） |
| `opengl_backend.h` | `OpenGLBackend` 类声明 |
| `render_backend.h` | `IRenderBackend` 接口 + `RenderSettings`（SwapChain 兼容层） |
| `material_types.h` | 材质参数类型枚举（Float/Vec2/Vec3/Vec4/Mat3/Mat4/Texture）与布局描述 |
| `shader_cache.h` / `.cpp` | Shader 编译缓存：按 (stage, sourceHash) 去重，同步编译，内存缓存 |
| `material.h` / `.cpp` | **材质系统核心**：Shader 引用 + CPU uniform 块 + 参数按名设置 + PSO 绑定 |
| `screenshot/screenshot.h` / `.cpp` | 调试用截图：RGBA8 像素写 PNG（stb_image_write，PRIVATE stb 依赖），配合 `IRHIDevice::readbackBackbuffer()` 使用 |
| `simple_cube_renderer.cpp` | 最小可行立方体渲染器：通过 `Material` + `GLRHIDevice` 绘制（批次 B 验证用）。**保留在仓库，但 2026-08-04 起主循环已改用 RenderFrameRunner，不再被 main 使用** |
| `simple_cube_renderer.h` | `SimpleCubeRenderer` 类声明 |
| `components/MeshAssetRef.h` | 主 World 组件：`MeshAssetRef`（`Handle<MeshAsset>`） |
| `components/MaterialAssetRef.h` | 主 World 组件：`MaterialAssetRef`（`Handle<MaterialAsset>`） |
| `components/Camera.h` | 主 World 组件：`Camera`（fov/near/far/ortho 参数） |
| `components/DirectionalLight.h` | 主 World 组件：`DirectionalLight`（direction/color/intensity/ambient，阶段 5a） |
| `components/SkySettings.h` | 主 World 组件：`SkySettings`（天空渐变 zenith/horizon 颜色 + enabled，阶段 5c） |
| `components/WorldAabb.h` | 主 World 组件：`WorldAABB`（世界空间包围盒，剔除用；包装 core `AABB`，保持数学库零 ECS 依赖） |
| `components/RenderComponents.h` | Render World 组件：`RenderMesh`, `RenderMaterial`, `RenderTransform`, `RenderAABB` |
| `components/RenderCamera.h` | Render World 组件：`ExtractedView`（view/proj/frustum/viewport/view_pos）+ `Rect` |
| `components/RenderLight.h` | Render World 组件：`ExtractedLight`（方向光快照，阶段 5a） |
| `components/RenderSky.h` | Render World 组件：`ExtractedSky`（天空设置快照，阶段 5c） |
| `RenderPhase.h` | 渲染阶段枚举：`ShadowMap`, `Opaque3D`, `AlphaMask`, `Transparent3D`, `UI` |
| `extract/MainWorldSync.h` | 主世界 ↔ 渲染世界实体双向映射表 |
| `render_world/RenderWorld.h/cpp` | Render World 容器：ECS World + ExtractSchedule + `MainWorldSync` |
| `render_world/ExtractSchedule.h/cpp` | Extract 阶段调度器：`IExtractSystem` 注册与顺序执行 |
| `extract/ExtractRenderablesSystem.h/cpp` | 搬运 `(MeshAssetRef, MaterialAssetRef, GlobalTransform)` → Render World |
| `extract/ExtractCameraSystem.h/cpp` | 搬运 `(Camera, GlobalTransform)` → `ExtractedView` |
| `extract/ExtractLightSystem.h/cpp` | 搬运第一个 `DirectionalLight` → `ExtractedLight`（阶段 5a） |
| `extract/ExtractSkySystem.h/cpp` | 搬运第一个 `SkySettings` → `ExtractedSky`（阶段 5c） |
| `culling/FrustumCullSystem.h/cpp` | 逐实体视锥剔除：`ExtractedView.frustum` vs `RenderAABB`；无 `RenderAABB` 则始终可见。**当实体数 > 256 且传入 `ThreadPool*` 时自动并行化** |
| `culling/ViewVisibleList.h` | Culling 阶段显式产出：可见实体列表 |
| `queue/PhaseItem.h` | 渲染阶段最小单元：`SortKey`（64-bit）+ `Entity` + `instance_count` |
| `queue/BinnedRenderPhase.h/cpp` | Opaque/AlphaMask 分箱：按 `material_id` 聚类减少状态切换 |
| `queue/SortedRenderPhase.h/cpp` | Transparent/UI 深度排序：远→近（`~depthBits`），使用 **64-bit 稳定基数排序** |
| `queue/QueueDrawsSystem.h/cpp` | 按 Phase 生成 Items：深度计算 + SortKey 构造 + 分箱/排序。**当可见实体数 > 256 且传入 `ThreadPool*` 时自动并行化** |
| `RenderResources.h` | Queue 阶段产出：`ViewBinnedPhases` + `ViewSortedPhases` |
| `execute/RenderExecuteSystem.h/cpp` | Execute 阶段：消费四个 Phase 容器发出 GPU draw call（GPU 资源由 Prepare 阶段解析）；每 draw 下发 uMVP/uModel/uNormalMatrix/uViewPos/光照 uniform（5a）；自持天空渐变 pass（全屏三角形 + 内联 sky shader，clear 后 opaque 前，5c） |
| `frame/RenderFrameRunner.h/cpp` | 帧驱动层：`runFrame()` 串联 Extract → Prepare → Cull → Queue → Execute，产出 `FrameStats`（含 PSO 缓存/显存计数，5c）；主循环唯一渲染入口 |

## 重要入口
- 改**RHI 抽象接口** → 动 `rhi_device.h` / `rhi_types.h`
- 改**OpenGL RHI 具体实现** → 动 `gl_rhi_device.h` / `.cpp` + `gl_command_translator.h` / `.cpp`
- 改**D3D12 RHI 具体实现** → 动 `d3d12_rhi_device.h` / `.cpp` + `d3d12_command_translator.h` / `.cpp`
- 改**命令缓冲格式/命令集** → 动 `render_command_buffer.h` / `deferred_command_list.h` / `.cpp`（两个翻译器需同步）
- 改**后端选择/着色器路径** → 动 `rhi_device_factory.cpp`（后端枚举 + `--backend=` 参数在 `launch/templates/main.cpp.in`）
- 改**GPU 资源生命周期（延迟删除 / Fence / 显存预算）** → 动 `rhi_resources.h/.cpp` / `rhi_device.h` / `gl_rhi_device.h/.cpp`
- 改**瞬态资源池** → 动 `rhi_transient_resource_pool.h/.cpp`
- 改**PSO 缓存策略** → 动 `rhi_pipeline.h` / `gl_rhi_device.cpp`
- 改**渲染后端接口（SwapChain/帧管理）** → 动 `render_backend.h`
- 改**OpenGL 具体实现（视口、清除色、VSync）** → 动 `opengl_backend.cpp`
- 改**最小立方体渲染** → 动 `simple_cube_renderer.cpp`（已不在主循环使用）
- 改**帧驱动 / 渲染管线编排** → 动 `frame/RenderFrameRunner.h/cpp`
- 改**GPU 绘制消费端（Execute 阶段）** → 动 `execute/RenderExecuteSystem.h/cpp`
- 改**Render World / Extract 流程** → 动 `render_world/RenderWorld.h/cpp` / `ExtractSchedule.h/cpp`
- 改**Extract 系统逻辑** → 动 `extract/ExtractRenderablesSystem.h/cpp` / `ExtractCameraSystem.h/cpp`
- 改**主世界渲染组件** → 动 `components/MeshAssetRef.h` / `MaterialAssetRef.h` / `Camera.h` / `WorldAabb.h`
- 改**渲染世界组件** → 动 `components/RenderComponents.h` / `RenderCamera.h`
- 改**视锥剔除** → 动 `culling/FrustumCullSystem.h/cpp`
- 改**Phase 队列** → 动 `queue/QueueDrawsSystem.h/cpp` / `BinnedRenderPhase.h/cpp` / `SortedRenderPhase.h/cpp`

## 依赖关系
- 向上依赖：
  - Window（依赖 IWindow 做 SwapBuffers）
  - CoreLib（HashMap、foundation_types）
- 被依赖：
  - Runtime（主循环调用 beginFrame / present）

## 架构决策
- **分层**：
  - `IRenderBackend` / `OpenGLBackend` 负责 SwapChain 和帧边界（上下文、Present、清屏）
  - `IRHIDevice` / `GLRHIDevice` 负责 GPU 资源创建、命令录制、Fence 与生命周期管理
  - `Material` 位于 RHI 之上，管理 shader + 参数 + PSO
  - 未来引入 D3D12/Vulkan 时，`IRenderBackend` 可能合并进 `IRHIDevice`
- `beginFrame()` 每帧自动查询窗口大小并设置 glViewport
- **GPU 资源生命周期**：
  - `GPUResource::release()` 不立即销毁，而是交给所属 `IRHIDevice` 的延迟删除队列
  - `IRHIDevice::signalFrame()` 每帧末尾插入 GPU Fence，`getCompletedFenceValue()` 非阻塞查询
  - `IRHIDevice::flushPendingDeletes()` 在确认 GPU 安全后批量真正释放资源
  - 显存占用由每个资源自行报告（`memorySizeBytes()`），设备汇总为 `getTrackedMemoryUsage()`；`queryMemoryInfo()` 在 OpenGL 上尝试 NVX/ATI 扩展
  - 当前为设备单例模型；ECS Resource 系统就绪后，预算/删除队列/瞬态池等可变状态应迁移为 World Resource（参考 TODO.md 债务项）

## 技术债务

> 统一维护于 [TODO.md](../../../../TODO.md)。本模块相关条目包括：Render/UniformBinding、Render/MaterialNoVariant、Render/ShaderSyncCompile、Render/D3D12MipGeneration、Render/D3D12SyncUpload、Render/D3D12PixMarkers、Render/D3D12NoImGui。

### 2026-08-07 已完成（6d D3D12 后端）
- ✅ `D3D12RHIDevice` + `D3D12CommandTranslator`：D3D12 后端落地，Sponza 画面与 GL 功能等价（~114 fps vs GL 60 vsync，剔除统计一致），`--backend=d3d12` 选择后端
- ✅ `PipelineStateDesc` 携带顶点输入布局（D3D12 PSO 必需，GL 忽略）；着色器字节码路径按后端选择
- ✅ 新增 `test_d3d12_backend.cpp` 冒烟测试（设备/buffer/纹理/fence/显存）
- ✅ 既知限制记入 TODO：D3D12 纹理无运行时 mip 生成（mip 0 only）、逐纹理同步上传偏慢、PIX 调试标注未接、D3D12 模式无 ImGui

### 2026-05-31 已完成（级别 1 轻量优化）
- ✅ `IRHICommandList::pushDebugGroup` / `popDebugGroup` / `insertDebugMarker` — GPU 调试标注接口（映射到 GL_KHR_debug / GL 4.3+）
- ✅ `GPUResource::setDebugName` — 资源对象命名（RenderDoc/PIX 可识别）
- ✅ `GLCommandList` Uniform Location 缓存 — `HashMap<(program, StringId), GLint>`，消除每 Draw Call 的 `glGetUniformLocation` 字符串查询
- ✅ `RHIErrorCode` 统一错误码枚举 — 为跨后端错误分类预留骨架

### 2026-06-19 已完成（GPU 资源生命周期管理）
- ✅ `GPUResource` 延迟删除：`release()` 将资源交回 `IRHIDevice`，`internalDestroy()` 在 GPU Fence 安全后调用
- ✅ `IRHIDevice` 新增 `signalFrame()` / `getCompletedFenceValue()` / `queueResourceForDelete()` / `flushPendingDeletes()` / `queryMemoryInfo()` / `getTrackedMemoryUsage()`
- ✅ `GLRHIDevice` OpenGL Fence 实现（`GL_SYNC_GPU_COMMANDS_COMPLETE`）、延迟删除队列、显存跟踪与 NVX/ATI 扩展查询
- ✅ `TransientTexturePool` 瞬态纹理复用池（按 `TextureDesc` 分组，Fence 保护回收）
- ✅ `SimpleCubeRenderer::endFrame()` 每帧调用 `signalFrame()` + `flushPendingDeletes()`
- ✅ 新增单元测试：`test_gpu_resource_lifecycle.cpp` 覆盖延迟删除、引用计数、瞬态池、显存大小计算
