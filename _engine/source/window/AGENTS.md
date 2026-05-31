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
  - [ImGui 模块](../imgui/AGENTS.md)
  - [Render 模块](../render/AGENTS.md)
  - [Runtime 模块](../../_game/source/runtime/AGENTS.md)

## 架构决策
- `InputQueue` 使用 `DynamicArray<RawInputEvent>` 存储事件，已消除 `std::vector` 依赖

## 技术债务

> 统一维护于 [TODO.md](../../../../TODO.md)。本模块相关条目包括：Window/SingleBackend、Window/VulkanStub。
