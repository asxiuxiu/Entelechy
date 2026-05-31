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
| `imgui_panels.cpp` | Debug 面板、Log 面板、ECS Inspector 面板（基于 `AtomRegistry` 零硬编码绘制）、DockSpace 构建 |
| `imgui_atom_registry.cpp` | ImGui 后端：`AtomRegistry::registerBuiltinAtoms()` 实现，注册原子类型的 Inspector 绘制函数 |
| `imgui_panels.h` | 面板构建函数声明、`WindowSizeRequest` 结构体 |

## 重要入口
- 改**ImGui 初始化/后端的配置** → 动 `imgui_manager.cpp`
- 改**Debug 面板的 UI 内容**（如新增分辨率按钮） → 动 `imgui_panels.cpp`
- 改**Log 面板的显示内容或过滤/滚动行为** → 动 `imgui_panels.cpp`
- 改**ECS Inspector 的实体列表、组件字段或交互行为** → 动 `imgui_panels.cpp`
- 改**Inspector 原子类型绘制注册** → 动 `imgui_atom_registry.cpp`
- 改**面板数据交互结构** → 动 `imgui_panels.h`

## 依赖关系
- 向上依赖：
  - Window（需要 GLFWwindow 句柄）
  - Render（需要 OpenGL 上下文）
  - Log（Log 面板读取 Logger 历史记录）
  - Core（ECS Inspector 读取 World、TypeRegistry、Entity）
  - System（ECS Inspector 驱动 Scheduler tick）
- 被依赖：
  - Runtime（主循环调用）

## 架构决策
- 目前开启 `ImGuiConfigFlags_DockingEnable`，关闭 `ViewportsEnable`（面板限制在窗口内）
- DPI 缩放通过 `glfwGetWindowContentScale` 自动适配
- **Inspector 零硬编码**：`drawField` 优先查询 `AtomRegistry`，其次硬编码展开 `Vec3`/`Quat`/`Mat4`/`Entity`，最后递归 `ComponentDesc`；新增组件类型无需修改 ImGui 代码即可显示

## 技术债务

> 统一维护于 [TODO.md](../../../../TODO.md)。
