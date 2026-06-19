# Render 模块
> 路径：`_engine/source/render`

## 一句话职责

图形渲染后端抽象与 OpenGL 实现，包含 RHI（Render Hardware Interface）抽象层骨架。

## 关键文件
| 文件 | 职责 |
|------|------|
| `rhi_types.h` | RHI 基础类型：Buffer/Texture/Shader 枚举、资源描述结构、渲染通道描述、`RHIFenceValue`、`RHIMemoryInfo` |
| `rhi_resources.h` | GPUResource 基类（引用计数 + 延迟删除支持）、RHIRef 智能句柄、具体资源类型声明 |
| `rhi_device.h` | `IRHIDevice`（资源工厂 + 提交 + Frame Fence + 延迟删除 + 显存预算）与 `IRHICommandList`（命令录制 + 调试标注）纯虚接口 |
| `rhi_pipeline.h` | `PipelineStateDesc`（完整 PSO 描述，含哈希支持）、`PSOManager`（全局缓存） |
| `rhi_transient_resource_pool.h` / `.cpp` | 瞬态纹理池：按描述符分组复用单帧生命周期纹理，预留显存别名接口 |
| `gl_rhi_device.h` / `.cpp` | OpenGL 后端对 RHI 接口的实现：`GLRHIDevice`、`GLCommandList`、GL 资源对象、Fence 跟踪、延迟删除队列、显存统计 |
| `rhi_resources.cpp` | `GPUResource::release()` 实现：引用计数归零后交给所属设备延迟删除 |
| `opengl_backend.cpp` | OpenGL 初始化、视口管理、清除与呈现（SwapChain 层，保留兼容） |
| `opengl_backend.h` | `OpenGLBackend` 类声明 |
| `render_backend.h` | `IRenderBackend` 接口 + `RenderSettings`（SwapChain 兼容层） |
| `material_types.h` | 材质参数类型枚举（Float/Vec2/Vec3/Vec4/Mat4/Texture）与布局描述 |
| `shader_cache.h` / `.cpp` | Shader 编译缓存：按 (stage, sourceHash) 去重，同步编译，内存缓存 |
| `material.h` / `.cpp` | **材质系统核心**：Shader 引用 + CPU uniform 块 + 参数按名设置 + PSO 绑定 |
| `simple_cube_renderer.cpp` | 最小可行立方体渲染器：通过 `Material` + `GLRHIDevice` 绘制（批次 B 验证用） |
| `simple_cube_renderer.h` | `SimpleCubeRenderer` 类声明 |
| `components/MeshHandle.h` | 主 World 组件：`MeshHandle`（mesh asset ID） |
| `components/MaterialHandle.h` | 主 World 组件：`MaterialHandle`（material asset ID） |
| `components/Camera.h` | 主 World 组件：`Camera`（fov/near/far/ortho 参数） |
| `components/RenderComponents.h` | Render World 组件：`RenderMesh`, `RenderMaterial`, `RenderTransform` |
| `components/RenderCamera.h` | Render World 组件：`ExtractedView`（view/proj/frustum/viewport）+ `Rect` |
| `RenderPhase.h` | 渲染阶段枚举：`ShadowMap`, `Opaque3D`, `AlphaMask`, `Transparent3D`, `UI` |
| `extract/MainWorldSync.h` | 主世界 ↔ 渲染世界实体双向映射表 |
| `render_world/RenderWorld.h/cpp` | Render World 容器：ECS World + ExtractSchedule + `MainWorldSync` |
| `render_world/ExtractSchedule.h/cpp` | Extract 阶段调度器：`IExtractSystem` 注册与顺序执行 |
| `extract/ExtractRenderablesSystem.h/cpp` | 搬运 `(MeshHandle, MaterialHandle, GlobalTransform)` → Render World |
| `extract/ExtractCameraSystem.h/cpp` | 搬运 `(Camera, GlobalTransform)` → `ExtractedView` |
| `culling/FrustumCullSystem.h/cpp` | 逐实体视锥剔除：`ExtractedView.frustum` vs `AABB`；无 AABB 则始终可见。**当实体数 > 256 且传入 `ThreadPool*` 时自动并行化** |
| `culling/ViewVisibleList.h` | Culling 阶段显式产出：可见实体列表 |
| `queue/PhaseItem.h` | 渲染阶段最小单元：`SortKey`（64-bit）+ `Entity` + `instance_count` |
| `queue/BinnedRenderPhase.h/cpp` | Opaque/AlphaMask 分箱：按 `material_id` 聚类减少状态切换 |
| `queue/SortedRenderPhase.h/cpp` | Transparent/UI 深度排序：远→近（`~depthBits`），使用 **64-bit 稳定基数排序** |
| `queue/QueueDrawsSystem.h/cpp` | 按 Phase 生成 Items：深度计算 + SortKey 构造 + 分箱/排序。**当可见实体数 > 256 且传入 `ThreadPool*` 时自动并行化** |
| `RenderResources.h` | Queue 阶段产出：`ViewBinnedPhases` + `ViewSortedPhases` |

## 重要入口
- 改**RHI 抽象接口** → 动 `rhi_device.h` / `rhi_types.h`
- 改**OpenGL RHI 具体实现** → 动 `gl_rhi_device.h` / `.cpp`
- 改**GPU 资源生命周期（延迟删除 / Fence / 显存预算）** → 动 `rhi_resources.h/.cpp` / `rhi_device.h` / `gl_rhi_device.h/.cpp`
- 改**瞬态资源池** → 动 `rhi_transient_resource_pool.h/.cpp`
- 改**PSO 缓存策略** → 动 `rhi_pipeline.h` / `gl_rhi_device.cpp`
- 改**渲染后端接口（SwapChain/帧管理）** → 动 `render_backend.h`
- 改**OpenGL 具体实现（视口、清除色、VSync）** → 动 `opengl_backend.cpp`
- 改**最小立方体渲染** → 动 `simple_cube_renderer.cpp`
- 改**Render World / Extract 流程** → 动 `render_world/RenderWorld.h/cpp` / `ExtractSchedule.h/cpp`
- 改**Extract 系统逻辑** → 动 `extract/ExtractRenderablesSystem.h/cpp` / `ExtractCameraSystem.h/cpp`
- 改**主世界渲染组件** → 动 `components/MeshHandle.h` / `MaterialHandle.h` / `Camera.h`
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

> 统一维护于 [TODO.md](../../../../TODO.md)。本模块相关条目包括：Render/RHIImmediateExecution、Render/UniformBinding、Render/MaterialNoVariant、Render/ShaderSyncCompile、Render/SingleBackend。

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
