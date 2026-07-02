# 构建系统规范

## 添加新模块

以 `_engine/source/newmod/` 为例：

### 1. 创建模块目录与 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME NewModLib
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/newmod.cpp
        ${CMAKE_CURRENT_LIST_DIR}/newmod.h
    PUBLIC_DEPS CoreLib
    # INIT_FUNCTION initNewMod   # 如需要在 main() 中初始化
    # NAMESPACE Entelechy        # 默认命名空间
)
```

**约束**：
- `SOURCES` 必须手动列出源文件（禁止 `file(GLOB)`）。
- 使用 `${CMAKE_CURRENT_LIST_DIR}` 引用本模块路径。
- `PUBLIC_DEPS` 传递依赖的 include path 和库；`PRIVATE_DEPS` 仅本模块内部使用。

### 2. 注册模块

在 `launch/cmake_projects.json`（引擎模块）或 `_game/cmake_projects.json`（游戏模块）中添加：

```json
{
    "name": "newmod",
    "path": "source/newmod",
    "enabled": true,
    "init_function": "initNewMod",
    "namespace": "Entelechy"
}
```

`init_function` / `namespace` 仅在模块需要在 `main()` 中初始化时填写。

### 3. 完成

无需修改任何生成器源码。重新运行：

```bash
python scripts/build/build.py --debug --build
```

CMake 会自动发现并链接新模块。

---

## 构建入口

首次构建前初始化项目本地环境（创建 `.venv` 并安装 `requirements.txt` 中的 pinned Conan）：

```bash
python scripts/tools/setup_env.py
```

```bash
# 默认配置 + 编译
python scripts/build/build.py --debug --build

# Release
python scripts/build/build.py --release --build
```

`build.py` 会优先使用 `.venv/Scripts/conan.exe`（Windows）或 `.venv/bin/conan`（Unix），避免受宿主机 Conan 版本影响。

### `--strict-build`（可选）

Windows 上 Conan 默认会把 `compiler.version=194` 的依赖 fallback 到 `compiler.version=193` 的预编译 binary。该兼容包在 Visual Studio Multi-config generator 下会导致 CMake 配置阶段打印 `IMPORTED_LOCATION not set...` 错误（不影响最终编译）。

`build.py` 支持 `--strict-build` 参数，强制从源码编译 host 依赖以生成与当前编译器完全匹配的 binary，从而消除上述噪音：

```bash
python scripts/build/build.py --debug --build --strict-build
```

> ⚠️ 当前环境受限：本机 Conan 配置中 `bmlib` 等 booming-inc 内部 remote 提供了一个定制版 `cmake/3.25.3.5`，其源码指向私有 GitLab。启用 `--strict-build` 时 Conan 会尝试重建 cmake 并触发无权限错误。因此 Zed / VS Code 的 Build 任务**没有默认启用** `--strict-build`，保持普通 `--build=missing` 流程以保证构建稳定。若未来需要启用 strict-build，需先解决 cmake 包来源问题（例如切换到 conan-center 的公共 cmake 包或采用项目级独立 Conan home）。

---

## 构建系统架构

- **根 `CMakeLists.txt`**：直接 `add_subdirectory(真实模块路径)`，不再生成代理层。
- **`cmake/EntelechyModule.cmake`**：提供 `entelechy_module()` / `entelechy_get_enabled_modules()` 宏，统一模块创建、注册和测试发现。
- **`launch/templates/main.cpp.in`**：由 CMake `configure_file` 生成入口文件，自动注入 init 函数前向声明和调用。
- **`launch/generator.py`**：已弃用，仅作为 standalone `main.cpp` 生成的 fallback 保留。

### Phase 1：统一模块声明（已完成）

引入 `entelechy_module()` 宏，消除代理层和硬编码链接列表，新增模块只需写模块 `CMakeLists.txt` + JSON 注册。

### Phase 2：清理模块边界（已完成）

- `entelechy_module()` 中 `${CMAKE_CURRENT_LIST_DIR}/..` 从 **PUBLIC** 降为 **PRIVATE**，模块不再对外泄露父目录。
- 所有跨模块 include 统一为**模块前缀风格**（如 `#include "log/log_macros.h"`、`#include "ecs/event/event_buffer.h"`）。CMake 在 `build/include/` 下为每个模块创建目录链接（Windows junction / Unix symlink），指向该模块的 `public/` 目录，全局 include 路径仅暴露 `build/include/`。
- 修复了一批历史遗留的隐式依赖（如 `EcsLib` 未声明 `LogLib` / `ThreadPoolLib` 依赖）。

---

## Include 风格规范

构建系统通过 `build/include/<module>/` → `<module>/public/` 的目录链接（junction / symlink）暴露公共头文件。因此代码中**永远不要在路径里写 `public`**，编译器看到的已经是 `public/` 下的内容。

| 场景 | 示例 | 说明 |
|------|------|------|
| 跨模块头文件 | `#include "ecs/world/phase.h"` | ✅ 标准写法，带模块前缀 |
| 模块内部 public 头文件 | `#include "ecs/world/phase.h"` | ✅ 同样使用模块前缀，与跨模块保持一致 |
| 含 `public` 的路径 | `#include "ecs/public/world/phase.h"` | ❌ 错误，链接已抹掉 `public` 层，路径无法解析 |
| 裸文件名 | `#include "phase.h"` | ❌ 已废弃，多个模块可能有同名子目录（如 `type/`），易产生歧义 |
| 相对路径 `../` | `#include "../world/phase.h"` | ❌ 禁止，头文件必须自包含且路径稳定 |

---

## 构建陷阱

- ❌ **不要直接修改 `build/generated/`**。该目录由 CMake configure 阶段生成。
- ❌ **不要在 `CMakeLists.txt` 中使用 `file(GLOB)`**。手动列出源文件以确保增量构建正确。
- ❌ **不要在模块 CMakeLists.txt 中硬编码链接列表**。依赖通过 `PUBLIC_DEPS` / `PRIVATE_DEPS` 声明，由 CMake 传递依赖自动解析。
