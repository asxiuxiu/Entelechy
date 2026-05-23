# Render 模块
> 路径：`_engine/source/render`

## 一句话职责
图形渲染后端抽象与 OpenGL 实现。

## 关键文件
| 文件 | 职责 |
|------|------|
| `opengl_backend.cpp` | OpenGL 初始化、视口管理、清除与呈现 |
| `opengl_backend.h` | `OpenGLBackend` 类声明 |
| `render_backend.h` | `IRenderBackend` 接口 + `RenderSettings` |
| `simple_cube_renderer.cpp` | 最小可行立方体渲染器：硬编码索引立方体网格 + MVP 着色器（批次 B 验证用） |
| `simple_cube_renderer.h` | `SimpleCubeRenderer` 类声明 |

## 重要入口
- 改**渲染后端接口** → 动 `render_backend.h`
- 改**OpenGL 具体实现**（视口、清除色、VSync） → 动 `opengl_backend.cpp`
- 改**最小立方体渲染** → 动 `simple_cube_renderer.cpp`

## 依赖关系
- 向上依赖：
  - Window（依赖 IWindow 做 SwapBuffers）
- 被依赖：
  - Runtime（主循环调用 beginFrame / present）

## 架构决策 / 临时约束
- 目前只有 OpenGL 后端，未来可能加入 Vulkan / Metal / D3D12
- `beginFrame()` 每帧自动查询窗口大小并设置 glViewport
