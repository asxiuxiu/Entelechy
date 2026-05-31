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

```bash
# 默认配置 + 编译
python scripts/build/build.py --debug --build

# Release
python scripts/build/build.py --release --build
```

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
- 所有跨模块 include 统一为**裸文件名风格**（如 `#include "log_macros.h"`、`#include "transform_component.h"`），依赖通过 `PUBLIC_DEPS` / `PRIVATE_DEPS` 的 include path 传递。
- 修复了一批历史遗留的隐式依赖（如 `EcsLib` 未声明 `LogLib` / `ThreadPoolLib` 依赖）。

---

## Include 风格规范

| 场景 | 示例 | 说明 |
|------|------|------|
| 模块内部子目录 | `#include "queue/PhaseItem.h"` | 合法，模块根目录在 include path 中 |
| 跨模块头文件 | `#include "log_macros.h"` | ✅ 推荐，裸文件名，依赖模块的 PUBLIC include path 传递 |
| 跨模块前缀 | `#include "log/log_macros.h"` | ❌ 已废弃，依赖 `..` 暴露 |

---

## 构建陷阱

- ❌ **不要直接修改 `build/generated/`**。该目录由 CMake configure 阶段生成。
- ❌ **不要在 `CMakeLists.txt` 中使用 `file(GLOB)`**。手动列出源文件以确保增量构建正确。
- ❌ **不要在模块 CMakeLists.txt 中硬编码链接列表**。依赖通过 `PUBLIC_DEPS` / `PRIVATE_DEPS` 声明，由 CMake 传递依赖自动解析。
