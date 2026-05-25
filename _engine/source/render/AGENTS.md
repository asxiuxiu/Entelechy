# Render 模块
> 路径：`_engine/source/render`

## 一句话职责
图形渲染后端抽象与 OpenGL 实现，包含 RHI（Render Hardware Interface）抽象层骨架。

## 关键文件
| 文件 | 职责 |
|------|------|
| `rhi_types.h` | RHI 基础类型：Buffer/Texture/Shader 枚举、资源描述结构、渲染通道描述 |
| `rhi_resources.h` | GPUResource 基类（引用计数）、RHIRef 智能句柄、具体资源类型声明 |
| `rhi_device.h` | `IRHIDevice`（资源工厂 + 提交）与 `IRHICommandList`（命令录制）纯虚接口 |
| `rhi_pipeline.h` | `PipelineStateDesc`（完整 PSO 描述，含哈希支持）、`PSOManager`（全局缓存） |
| `gl_rhi_device.h` / `.cpp` | OpenGL 后端对 RHI 接口的实现：`GLRHIDevice`、`GLCommandList`、GL 资源对象 |
| `opengl_backend.cpp` | OpenGL 初始化、视口管理、清除与呈现（SwapChain 层，保留兼容） |
| `opengl_backend.h` | `OpenGLBackend` 类声明 |
| `render_backend.h` | `IRenderBackend` 接口 + `RenderSettings`（SwapChain 兼容层） |
| `simple_cube_renderer.cpp` | 最小可行立方体渲染器：硬编码索引立方体网格 + MVP 着色器（批次 B 验证用） |
| `simple_cube_renderer.h` | `SimpleCubeRenderer` 类声明 |

## 重要入口
- 改**RHI 抽象接口** → 动 `rhi_device.h` / `rhi_types.h`
- 改**OpenGL RHI 具体实现** → 动 `gl_rhi_device.h` / `.cpp`
- 改**PSO 缓存策略** → 动 `rhi_pipeline.h` / `gl_rhi_device.cpp`
- 改**渲染后端接口（SwapChain/帧管理）** → 动 `render_backend.h`
- 改**OpenGL 具体实现（视口、清除色、VSync）** → 动 `opengl_backend.cpp`
- 改**最小立方体渲染** → 动 `simple_cube_renderer.cpp`

## 依赖关系
- 向上依赖：
  - Window（依赖 IWindow 做 SwapBuffers）
  - BaseLib（HashMap、foundation_types）
- 被依赖：
  - Runtime（主循环调用 beginFrame / present）

## 架构决策 / 临时约束
- **RHI 抽象层（Phase 1）**：
  - 接口设计对齐 UE 的 `FRHICommandList` 意图（命令式录制、预留延迟执行空间）
  - OpenGL 后端采用**即时执行**实现（Phase 1 简化），但接口不暴露即时语义
  - 资源生命周期：引用计数 (`GPUResource`) + `RHIRef` 智能句柄，延迟删除队列为未来扩展预留
  - PSO 管理：运行时全局哈希缓存 (`PSOManager`)，异步编译架构预留
- **分层**：
  - `IRenderBackend` / `OpenGLBackend` 负责 SwapChain 和帧边界（上下文、Present、清屏）
  - `IRHIDevice` / `GLRHIDevice` 负责 GPU 资源创建和命令录制
  - 未来引入 D3D12/Vulkan 时，`IRenderBackend` 可能合并进 `IRHIDevice`
- 目前只有 OpenGL 后端，未来可能加入 Vulkan / Metal / D3D12
- `beginFrame()` 每帧自动查询窗口大小并设置 glViewport
