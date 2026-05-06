# ImGui 模块
> 路径：`_engine/source/imgui`

## 一句话职责
Dear ImGui 生命周期封装、面板构建、Docking 布局。

## 关键文件
| 文件 | 职责 |
|------|------|
| `imgui_init.cpp` | 初始化桩实现 |
| `imgui_init.h` | 初始化桩（实际逻辑在 ImGuiManager） |
| `imgui_manager.cpp` | ImGui 上下文创建、GLFW+OpenGL3 后端初始化、DPI 缩放适配 |
| `imgui_manager.h` | `ImGuiManager` 类声明 |
| `imgui_panels.cpp` | Debug 面板、Log 面板、DockSpace 构建 |
| `imgui_panels.h` | 面板构建函数声明、`WindowSizeRequest` 结构体 |

## 重要入口
- 改**ImGui 初始化/后端的配置** → 动 `imgui_manager.cpp`
- 改**Debug 面板的 UI 内容**（如新增分辨率按钮） → 动 `imgui_panels.cpp`
- 改**Log 面板的显示内容或过滤/滚动行为** → 动 `imgui_panels.cpp`
- 改**面板数据交互结构** → 动 `imgui_panels.h`

## 依赖关系
- 向上依赖：
  - Window（需要 GLFWwindow 句柄）
  - Render（需要 OpenGL 上下文）
  - Log（Log 面板读取 Logger 历史记录）
- 被依赖：
  - Runtime（主循环调用）

## 架构决策 / 临时约束
- 目前开启 `ImGuiConfigFlags_DockingEnable`，关闭 `ViewportsEnable`（面板限制在窗口内）
- DPI 缩放通过 `glfwGetWindowContentScale` 自动适配
