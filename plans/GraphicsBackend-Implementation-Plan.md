# 图形后端实施计划（Graphics Backend Implementation Plan）

> **前置依赖**：窗口系统已就绪（`window` 模块，GLFW）
> **目标**：先以 OpenGL 4.6 Core 作为过渡后端，快速接入 ImGui；后期通过 RHI 抽象层切换为 Vulkan
> **参考知识源**：`Notes/SelfGameEngine/Hello-Engine-Window/最简图形后端.md`、`ImGui 集成与调试面板.md`

---

## ✅ 已完成（当前状态）

| 阶段 | 内容 | 状态 |
|------|------|------|
| 阶段 0 | GLFW 窗口系统（创建、事件、Resize、关闭） | ✅ |
| 阶段 0 | 输入队列（键盘、鼠标、窗口事件） | ✅ |
| 阶段 0 | 日志系统（Console / File / JsonL） | ✅ |
| 阶段 0 | ECS 骨架（World、Component、Query） | ✅ |

---

## 架构决策：OpenGL → RHI → Vulkan

### 为什么阶段 1 不直接上 Vulkan？

Vulkan 最小初始化需要 ~600 行代码（Instance、Surface、Device、SwapChain、CommandPool、Fence/Semaphore……），而阶段 1 的目标只是"在屏幕上画出第一个颜色，并能接入 ImGui"。OpenGL 只需 ~30 行即可达到同等可见效果。

### 过渡策略

```
阶段 1（现在）
    └── OpenGL 4.6 Core 直接渲染
        ├── 清屏 + VSync + 自动适配 Resize
        ├── ImGui (glfw + opengl3 backends)
        └── 与 ECS 解耦：RenderSystem 读取 Window/RenderSettings 组件

阶段 5（后期）
    └── RHI 抽象层
        ├── 接口：IRenderDevice、IRenderPass、ISwapChain……
        ├── OpenGL 后端实现 → 降级为"简单后端"
        └── Vulkan 后端实现 → 升级为"主后端"
        └── 上层（ECS 渲染系统）无感知切换
```

> **参考 UE**：`FDynamicRHI` 统一接口，D3D12 / Vulkan / Metal 继承同一套抽象，运行时不关心底层 API。

---

## 阶段 1：OpenGL 最小图形后端

**目标**：让窗口从"黑屏"变成"可控颜色的屏幕"，建立 `render` 模块，接入现有构建系统。

**验收标准**：
- [ ] 运行后窗口显示深灰色（非系统默认的惨白/黑）
- [ ] 窗口 Resize 时渲染区域自动适配
- [ ] VSync 默认开启，不撕裂
- [ ] 帧时间 / FPS 可观测

### Task 1.1 引入 OpenGL 加载器（GLAD）

**方案**：通过 Conan 引入 `glad` 包，与现有 `glfw/3.4` 保持一致的管理方式。

```python
# conanfile.py
def requirements(self):
    self.requires("glfw/3.4")
    self.requires("glad/0.1.36")
```

> **为什么选 GLAD 而不是 GLEW**：GLAD 支持按需加载指定版本+扩展，生成体积更小；本项目只需要 OpenGL 4.6 Core，用 GLAD 更干净。

**验证**：重新运行 `python scripts/build/build.py`，确认 `compile_commands.json` 中能找到 `glad/glad.h`。

### Task 1.2 修改窗口模块，创建 OpenGL Context

当前 `glfw_window.cpp` 在 `create()` 中设置了 `GLFW_CLIENT_API, GLFW_NO_API`（为 Vulkan 预留）。阶段 1 需要先创建 OpenGL Context。

**改动点**：

1. `glfw_window.cpp` 的 `create()` 前追加 OpenGL hints：
   ```cpp
   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
   glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
   // macOS 需要：glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
   ```
   并移除 `GLFW_CLIENT_API, GLFW_NO_API`。

2. 创建窗口后调用 `glfwMakeContextCurrent(m_window)`。

3. `IWindow` 接口追加：
   ```cpp
   virtual void swapBuffers() = 0;
   virtual void makeContextCurrent() = 0;
   ```
   `GlfwWindow` 实现为 `glfwSwapBuffers(m_window)` / `glfwMakeContextCurrent(m_window)`。

> **诚实边界**：这会让窗口暂时"绑定"到 OpenGL。阶段 5 写 RHI 时，窗口创建逻辑会重构为"根据后端类型选择 API"，但阶段 1 不需要过度设计。

### Task 1.3 新建 `render` 模块

目录结构：

```
_engine/source/render/
├── render_backend.h          # 图形后端接口（阶段 1 只有 OpenGL 实现）
├── opengl_backend.cpp
├── opengl_backend.h
├── render_init.cpp           # initGraphics() / shutdownGraphics()
├── render_init.h
└── CMakeLists.txt
```

**`CMakeLists.txt` 示例**：

```cmake
cmake_minimum_required(VERSION 3.20)

find_package(glad REQUIRED)

add_library(RenderLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/opengl_backend.cpp
    ${CMAKE_CURRENT_LIST_DIR}/render_init.cpp
)

target_include_directories(RenderLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(RenderLib PUBLIC cxx_std_20)

target_link_libraries(RenderLib PUBLIC
    glad
    glfw
    WindowLib
    LogLib
)
```

**构建系统注册**：

1. `launch/cmake_projects.json` 中新增 `render` 模块：
   ```json
   {
       "name": "render",
       "path": "source/render",
       "enabled": true
   }
   ```

2. `launch/generator.py` 的 `link_libs` 区域追加：
   ```python
   elif 'render' in m['name'].lower():
       link_libs.append('RenderLib')
   ```

3. `generator.py` 的 `generate_main_cpp` 中追加：
   ```python
   elif 'render' in module_name.lower():
       forward_decls.append("namespace Entelechy { void initGraphics(); }")
       init_calls.append("    Entelechy::initGraphics();")
   ```

### Task 1.4 实现 OpenGL 最小后端

`opengl_backend.h`：

```cpp
#pragma once

namespace Entelechy {

struct RenderSettings {
    float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f};
    bool vsync = true;
};

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;
    virtual bool init(void* nativeWindow) = 0;
    virtual void shutdown() = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void setViewport(int x, int y, int width, int height) = 0;
    virtual void setClearColor(float r, float g, float b, float a) = 0;
    virtual void clear() = 0;
    virtual void present() = 0;
};

} // namespace Entelechy
```

`opengl_backend.cpp` 核心逻辑：

```cpp
bool OpenGLBackend::init(void* nativeWindow) {
    GLFWwindow* window = static_cast<GLFWwindow*>(nativeWindow);
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG_ERROR(..., "GLAD init failed");
        return false;
    }

    LOG_INFO(..., "OpenGL %s | GPU: %s",
        glGetString(GL_VERSION),
        glGetString(GL_RENDERER));

    glfwSwapInterval(settings.vsync ? 1 : 0);

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    return true;
}

void OpenGLBackend::beginFrame() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
}

void OpenGLBackend::clear() {
    glClearColor(settings.clearColor[0], ...);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLBackend::present() {
    glfwSwapBuffers(window);
}
```

### Task 1.5 接入主循环（main.cpp）

修改 `launch/templates/main.cpp`：

```cpp
// 新增头文件
#include "render/opengl_backend.h"

int main() {
    // ... init modules ...

    auto window = std::make_unique<Entelechy::GlfwWindow>();
    if (!window->create(1280, 720, "Entelechy")) { ... }

    // 初始化图形后端
    auto render = std::make_unique<Entelechy::OpenGLBackend>(window.get());
    if (!render->init()) { ... }

    while (!window->shouldClose()) {
        // ... poll events ...

        render->beginFrame();
        render->clear();
        // TODO: 阶段 2 在这里画 ImGui
        render->present();

        // ... FPS 统计 ...
    }

    render->shutdown();
    window->destroy();
}
```

> **注意**：当前 `main.cpp` 在 `launch/templates/` 下，是生成出来的。修改模板后需要重新运行 `python scripts/build/build.py` 生效。

### Task 1.6 Resize 安全处理

OpenGL 的 Resize 只需要每帧同步 `glViewport`（已在 `beginFrame` 中处理）。不需要像 Vulkan 那样重建 SwapChain。

---

## 阶段 2：ImGui 集成

**目标**：在颜色窗口上叠加一个可交互的调试 UI（Docking + 日志面板 + FPS 显示）。

**验收标准**：
- [ ] 屏幕上有"Debug"面板，显示 FPS 和帧时间
- [ ] 有"Logs"面板，实时显示最近日志（阻塞日志计划中的 Task 1.1）
- [ ] 支持 Docking：面板可拖拽停靠
- [ ] 支持中文显示（可选，阶段 1 可先加载基础 Latin 字符集）

### Task 2.1 引入 ImGui

**方案**：Conan 引入 `imgui` 包，并手动补充 backend 文件（如果 Conan 包未包含）。

```python
# conanfile.py
def requirements(self):
    self.requires("glfw/3.4")
    self.requires("glad/0.1.36")
    self.requires("imgui/1.91.0")
```

ImGui 官方 backend 文件：
- `imgui_impl_glfw.cpp/.h`
- `imgui_impl_opengl3.cpp/.h`

若 Conan 包内已包含（通常位于 `res/bindings/` 或类似路径），直接在 `CMakeLists.txt` 中引用；若未包含，从官方仓库拷贝到 `_engine/source/render/imgui_backends/`。

### Task 2.2 封装 ImGui 管理器

在 `render` 模块中新增 `imgui_manager.cpp/.h`：

```cpp
class ImGuiManager {
public:
    bool init(GLFWwindow* window);
    void shutdown();
    void newFrame();
    void render();
    void buildUI(float dt, float fps);  // 实际 UI 构建，可扩展
};
```

初始化：
```cpp
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGuiIO& io = ImGui::GetIO();
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // 阶段 1 暂不开，减少复杂度

ImGui::StyleColorsDark();
ImGui_ImplGlfw_InitForOpenGL(window, true);
ImGui_ImplOpenGL3_Init("#version 460");
```

### Task 2.3 渲染循环中插入 ImGui

每帧顺序（**严禁打乱**）：

```cpp
// 1. ImGui 新帧
imgui.newFrame();

// 2. 构建 UI（所有 ImGui::Begin/End 在这里）
imgui.buildUI(dt, fps);

// 3. 生成绘制数据
ImGui::Render();

// 4. 清屏 + 3D 场景（阶段 5 才有）
render->clear();

// 5. 绘制 ImGui（必须在 clear 之后，swap 之前）
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

// 6. 交换缓冲
render->present();
```

### Task 2.4 接入日志面板

- `ImGuiManager::buildUI()` 中绘制 `Begin("Logs")` 窗口
- 从 `Logger::instance().history()` 读取最近 N 条日志
- 不同级别用不同颜色：`ImGui::TextColored(red, "[ERROR] ...")`
- 自动滚动到底部（当用户未手动上滚时）

> 此任务与 `LogSystem-Implementation-Plan.md` 的 **Task 1.1（ImGui 日志面板）** 直接对接。图形后端负责"提供渲染循环和 ImGui 环境"，日志系统负责"提供数据与过滤逻辑"。

### Task 2.5 字体配置（可选）

阶段 1 可先用 ImGui 内置的 ProggyClean（13px，仅 Latin）。如需中文：

```cpp
static const ImWchar ranges[] = {
    0x0020, 0x00FF,  // Basic Latin
    0x4E00, 0x9FFF,  // CJK Unified Ideographs
    0
};
io.Fonts->AddFontFromFileTTF("Assets/fonts/NotoSansCJK-Regular.ttc", 18.0f, nullptr, ranges);
```

---

## 阶段 3：RHI 抽象与 Vulkan 迁移（远期规划）

**目标**：当 ECS 渲染管线、资源系统、多线程架构就绪后，用 RHI 层替换直接 OpenGL 调用，使上层系统无需关心底层 API。

**状态**：⏸️ **长期规划** —— 当前阶段 1~2 足够支撑调试和工具开发。

### Task 3.1 RHI 接口设计

参考 UE `FDynamicRHI` 模式：

```cpp
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;
    virtual bool init(const RenderDeviceInitParams& params) = 0;
    virtual void shutdown() = 0;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;

    virtual void setViewport(int x, int y, int w, int h) = 0;
    virtual void setScissor(int x, int y, int w, int h) = 0;
    virtual void clearRenderTarget(IRenderTarget* rt, const float* color) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t startIndex) = 0;

    // 资源创建（阶段 5 扩展）
    virtual IBuffer* createBuffer(...) = 0;
    virtual ITexture* createTexture(...) = 0;
    virtual IShader* createShader(...) = 0;
    virtual IPipelineState* createPipelineState(...) = 0;
};
```

### Task 3.2 OpenGL RHI 后端

将当前 `OpenGLBackend` 重构为 `OpenGLRenderDevice : public IRenderDevice`。将 `glClearColor` / `glClear` / `glViewport` 等调用封装到接口后面。

> **好处**：阶段 1 写的 OpenGL 代码不是白写——它们会变成 RHI 的一个具体实现。

### Task 3.3 Vulkan RHI 后端

新建 `VulkanRenderDevice : public IRenderDevice`：

- `VkInstance` / `VkSurfaceKHR` / `VkPhysicalDevice` / `VkDevice`
- `VkSwapchainKHR` + `VkImageView`
- `VkCommandPool` / `VkCommandBuffer`（每帧循环）
- `VkSemaphore`（imageAvailable / renderFinished）+ `VkFence`（CPU-GPU 同步）
- 双缓冲 / 三缓冲帧资源（`MAX_FRAMES_IN_FLIGHT = 2`）

Vulkan Resize 处理：
- `vkAcquireNextImageKHR` 返回 `VK_ERROR_OUT_OF_DATE_KHR` 时触发重建
- 等待 GPU 空闲 → 销毁旧 SwapChain / ImageView / Framebuffer → 按新尺寸重建

### Task 3.4 上层无感知切换

运行时通过命令行或配置文件选择后端：

```cpp
// main.cpp
std::unique_ptr<IRenderDevice> render;
if (useVulkan) {
    render = std::make_unique<VulkanRenderDevice>();
} else {
    render = std::make_unique<OpenGLRenderDevice>();
}
render->init(params);
```

ECS 渲染系统（`RenderSceneSystem`）只操作 `IRenderDevice*` 和 `RHICommandList`，不直接调用任何 OpenGL / Vulkan API。

### Task 3.5 ImGui 多后端支持

ImGui 需要对应后端的 Renderer Backend：
- OpenGL：`imgui_impl_opengl3.cpp`
- Vulkan：`imgui_impl_vulkan.cpp`（需要传入 `VkDevice`、`VkPipeline`、`VkDescriptorPool` 等）

在 `ImGuiManager` 中根据当前 RHI 后端初始化对应的 Renderer Backend。

---

## 构建系统改动清单

按执行顺序排列：

| 文件 | 改动内容 |
|------|---------|
| `conanfile.py` | 添加 `glad/0.1.36` 和 `imgui/1.91.0` |
| `_engine/source/window/glfw_window.cpp` | 移除 `GLFW_NO_API`，添加 OpenGL 4.6 hints，实现 `swapBuffers` / `makeContextCurrent` |
| `_engine/source/window/window.h` | 接口追加 `swapBuffers()`、`makeContextCurrent()` |
| `_engine/source/render/` | **新建模块**，含 OpenGL 后端 + ImGui 封装 |
| `_engine/source/render/CMakeLists.txt` | 链接 `glad`、`glfw`、`WindowLib`、`LogLib` |
| `launch/cmake_projects.json` | 注册 `render` 模块 |
| `launch/generator.py` | `link_libs` 和 `generate_main_cpp` 中注册 `RenderLib` |
| `launch/templates/main.cpp` | 接入 `initGraphics()`、渲染循环、ImGui 调用 |

---

## 阶段对照速查表

| 阶段 | 核心能力提升 | 对应笔记章节 | 推荐时机 |
|------|-------------|-------------|---------|
| 阶段 1 | 屏幕有颜色、VSync、Resize 自适应、FPS 显示 | 最简图形后端 — Step 1~5 | **立即执行** |
| 阶段 2 | ImGui 调试面板、Docking、日志可视化 | ImGui 集成 — Step 1~5 | **阶段 1 完成后立即执行** |
| 阶段 3 | RHI 抽象、OpenGL/Vulkan 双后端、上层无感知切换 | 最简图形后端 — Step 6~7 | ⏸️ ECS 渲染管线就绪后 |

---

## 风险与注意事项

### ⚠️ 1. `GLFW_NO_API` 的兼容性问题

当前 `window` 模块为了 Vulkan 预留了 `GLFW_NO_API`。阶段 1 改为 OpenGL Context 后，**阶段 3 需要重构窗口创建逻辑**，让它根据后端类型选择是否设置 `GLFW_NO_API`。

**缓解**：阶段 1 的改动保持最小，只在 `create()` 中改 hints，不要过度抽象。等阶段 3 再统一设计"窗口-后端"初始化协议。

### ⚠️ 2. ImGui 后端文件来源

Conan 的 `imgui` 包版本差异较大，有些版本不包含 `imgui_impl_*.cpp`。如果 Conan 包缺少，从 [官方仓库](https://github.com/ocornut/imgui/tree/master/backends) 下载对应版本的后端文件，放在 `render` 模块内直接编译。

### ⚠️ 3. OpenGL 状态机污染

OpenGL 是全局状态机。阶段 1 代码少，不容易出问题；阶段 2 接入 ImGui 后，ImGui 会修改 VAO、Shader、Blend 等状态。如果后续在 ImGui 之前画 3D，需要在 `ImGui::Render()` 前保存/恢复关键状态，或干脆规定"ImGui 最后画"（已在计划中规定）。

### ⚠️ 4. macOS 兼容性

OpenGL 4.6 Core 在 macOS 上**不可用**（Apple 只支持到 4.1）。如果需要在 macOS 开发，阶段 1 需要降级到 4.1，或阶段 1 就直接在 macOS 上用 Vulkan（MoltenVK）。**本项目当前主力开发平台是 Windows**，可以先写 4.6，macOS 支持作为阶段 3 RHI 的任务。

### ⚠️ 5. 多线程渲染

OpenGL Context 是线程绑定的，不能直接在非主线程调用 GL 命令。阶段 1~2 完全单线程渲染即可。阶段 3 Vulkan 才需要考虑 Render Thread + Command Buffer 的多线程提交。

---

## 下一步行动

1. **用户确认本计划**（尤其是 OpenGL 4.6 vs 4.1 的平台策略）
2. **执行 Task 1.1 ~ 1.3**：依赖 + 模块 + 构建系统注册
3. **执行 Task 1.4 ~ 1.6**：OpenGL 后端实现 + 主循环接入
4. **验证**：运行 `python scripts/build/build.py`，窗口应显示深灰色而非黑屏
5. **进入阶段 2**：接入 ImGui
