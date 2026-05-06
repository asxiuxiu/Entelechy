# Window 模块
> 路径：`_engine/source/window`

## 一句话职责
窗口创建与生命周期管理、输入事件收集、显示器与 DPI 查询。

## 关键文件
| 文件 | 职责 |
|------|------|
| `glfw_window.cpp` | GLFW 实现；窗口创建、事件回调、显示器工具函数 |
| `glfw_window.h` | GLFW 实现声明；含显示器分辨率 / DPI / 窗口居中查询 |
| `input_event.h` | `RawInputEvent` 统一输入事件结构（键盘/鼠标/窗口事件） |
| `input_queue.cpp` | 事件队列实现 |
| `input_queue.h` | 单例事件队列声明 |
| `window.h` | `IWindow` 抽象接口 |
| `window_init.cpp` | GLFW 库初始化/关闭实现 |
| `window_init.h` | GLFW 库初始化/关闭声明 |

## 重要入口
- 改**窗口默认大小 / 居中 / DPI 检测** → 动 `glfw_window.cpp`
- 改**输入事件类型或字段** → 动 `input_event.h`
- 改**事件分发或队列行为** → 动 `input_queue.cpp`

## 依赖关系
- 向上依赖：
  - 无（基础层）
- 被依赖：
  - SystemLib
  - Runtime（主循环消费事件）

## 架构决策 / 临时约束
- `IWindow` 目前只有 GLFW 实现，未来可能加入 SDL / Win32 后端
- `getNativeDisplay()` 是 Vulkan 创建 Surface 的预留 stub
