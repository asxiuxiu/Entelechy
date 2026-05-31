# 构建体系 UE 化改进计划

## Context

当前 Entelechy 的构建体系存在以下核心痛点：
1. **Python 生成器 + 代理 CMake 层过度复杂**：`generator.py` 生成 `build/sourcetree/` 代理目录，代理文件使用绝对路径、IDE 跳转异常、不可移植。
2. **双重硬编码**：`generator.py` 中用子字符串匹配推断 target 名（`link_libs`）和 init 函数名（`generate_main_cpp`），新增模块必须改生成器源码。
3. **可执行文件硬编码链接所有模块**：`@LINK_LIBS@` 列出全部 14 个库，没有利用 CMake 传递依赖。
4. **模块边界模糊**：多个模块通过 `target_include_directories(... PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)` 暴露整个 `_engine/source/`，跨模块裸 include 泛滥。
5. **零 CMake 宏复用**：每个模块的 CMakeLists.txt 都是复制-粘贴模式。
6. **游戏项目分离度不足**：`_game` 与 `_engine` 的耦合通过硬编码完成。

用户希望借鉴 **UE 的构建体系**（模块元数据驱动、public/private 边界、依赖自动解析、Target 概念），但保持 CMake 作为构建工具。

## 推荐方案：分三阶段渐进重构

每阶段结束后都能构建成功，降低风险。

---

## Phase 1：引入 `entelechy_module()` 宏 + 去掉代理层 + 统一模块声明

### 目标
消除代理层和硬编码链接列表，统一模块创建逻辑，**保持现有 include 方式兼容**（仍暴露 `..` 以兼容当前跨模块 include）。

### 1.1 新增 `cmake/EntelechyModule.cmake`

提供统一模块创建宏 `entelechy_module()`，设计如下：

```cmake
function(entelechy_module)
    cmake_parse_arguments(MOD "NO_TESTS" "NAME;TYPE;INIT_FUNCTION;NAMESPACE" "SOURCES;PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})
    # 验证 NAME、SOURCES 必填
    # 创建 target（默认 STATIC，支持 EXECUTABLE/OBJECT）
    # target_compile_features(... PUBLIC cxx_std_20)
    # target_include_directories(... PUBLIC ${CMAKE_CURRENT_LIST_DIR})
    # Phase 1 兼容：额外 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..（保持现有跨模块 include 工作）
    # target_link_libraries(... PUBLIC/PRIVATE)
    # 注册全局属性：ENTELECHY_ENABLED_MODULES、ENTELECHY_INIT_FUNCTIONS
    # 自动添加 tests/ 子目录（条件同现有逻辑）
    # 支持 NO_TESTS
endfunction()

function(entelechy_get_enabled_modules out_var)
    # 读取 GLOBAL PROPERTY ENTELECHY_ENABLED_MODULES
endfunction()
```

**参数设计**：
- `NAME`：CMake target 名（如 `CoreLib`）
- `TYPE`：库类型（`STATIC`/`EXECUTABLE`/`OBJECT`，默认 `STATIC`）
- `SOURCES`：源文件列表（手动列出，不使用 `file(GLOB)`）
- `PUBLIC_DEPS` / `PRIVATE_DEPS`：依赖 target 列表
- `INIT_FUNCTION`：初始化函数名（如 `initCore`）
- `NAMESPACE`：C++ 命名空间（如 `Entelechy`，默认 `Entelechy`）
- `NO_TESTS`：跳过自动添加 tests 子目录

### 1.2 重写顶层 `CMakeLists.txt`

去掉代理层，新结构：

```cmake
cmake_minimum_required(VERSION 3.20)
project(Entelechy LANGUAGES CXX)

# 1. 基础设置：C++20、MSVC UTF-8、Shipping、统一输出目录
# 2. include(cmake/EntelechyModule)
# 3. find_package(glad/glfw3/imgui) 提到顶层（避免各模块重复 find）
# 4. 读取 configs/full_build.json，解析 source_tree 数组
# 5. 对每个 repo 的 cmake_projects.json：
#      遍历 modules，对 enabled=true 的模块直接 add_subdirectory(${真实路径})
# 6. 收集所有模块的 init 函数，configure_file(launch/templates/main.cpp.in → build/generated/main.cpp)
# 7. add_executable(Entelechy build/generated/main.cpp)
# 8. 收集所有非测试模块作为链接目标，target_link_libraries(Entelechy PRIVATE ${所有模块})
```

**关键决策**：顶层直接 `add_subdirectory(真实模块路径)`，不再生成 `build/sourcetree/`。
`build/sourcetree/` 目录在下次构建前手动删除或忽略。

### 1.3 改造 `launch/templates/main.cpp` → `main.cpp.in`

重命名为 `.in` 文件，供 CMake `configure_file(@ONLY)` 使用。变量保持：
- `@SOLUTION_NAME@`
- `@FORWARD_DECLS@`
- `@INIT_CALLS@`

### 1.4 简化 `launch/generator.py`

**删除**：
- 代理 `CMakeLists.txt` 生成逻辑
- `link_libs` 硬编码映射
- `generate_modules_cmake`
- 根 `CMakeLists.txt` 生成

**保留并简化**：
- 只负责生成 `main.cpp`（作为 CMake configure 阶段的 fallback，或完全由 CMake 接管）
- 从 JSON 的 `init_function` 字段读取 init 函数（不再子字符串匹配）

**建议**：完全移除 `generator.py` 的调用，由 CMake 的 `configure_file` 生成 `main.cpp`。这样外部游戏项目只需要 `add_subdirectory(引擎路径)` 即可，无需运行 Python。

### 1.5 为 JSON 模块配置扩展元数据字段

在 `launch/cmake_projects.json` 和 `_game/cmake_projects.json` 中，为需要 init 的模块添加：

```json
{
    "name": "core",
    "path": "source/core",
    "enabled": true,
    "init_function": "Entelechy::initCore"
}
```

这样 `generator.py`（或 CMake）可直接读取，无需硬编码推断。

### 1.6 重构所有模块 `CMakeLists.txt`

全部改为调用 `entelechy_module()`。示例（`core/CMakeLists.txt`）：

**重构前**：
```cmake
cmake_minimum_required(VERSION 3.20)
add_library(CoreLib STATIC ...30+ files...)
target_include_directories(CoreLib PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(CoreLib PUBLIC MemoryLib)
target_compile_features(CoreLib PUBLIC cxx_std_20)
if(NOT SHIPPING_BUILD AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/tests")
    add_subdirectory(...)
endif()
```

**重构后**：
```cmake
cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME CoreLib
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/world.cpp
        ${CMAKE_CURRENT_LIST_DIR}/world.h
        ...
    PUBLIC_DEPS MemoryLib
    INIT_FUNCTION initCore
    NAMESPACE Entelechy
)
```

**需要重构的模块**（共 17 个）：
- `_engine/source/base/CMakeLists.txt`
- `_engine/source/core/CMakeLists.txt`
- `_engine/source/motor/CMakeLists.txt`
- `_engine/source/bridge/CMakeLists.txt`
- `_engine/source/math/CMakeLists.txt`
- `_engine/source/memory/CMakeLists.txt`
- `_engine/source/window/CMakeLists.txt`
- `_engine/source/log/CMakeLists.txt`
- `_engine/source/render/CMakeLists.txt`
- `_engine/source/imgui/CMakeLists.txt`
- `_engine/source/vfs/CMakeLists.txt`
- `_engine/source/asset/CMakeLists.txt`
- `_engine/source/thread_pool/CMakeLists.txt`
- `_engine/source/test/CMakeLists.txt`
- `_engine/source/test_runner/CMakeLists.txt`
- `_game/source/runtime/CMakeLists.txt`

### 1.7 重构 `test_runner/CMakeLists.txt`

**删除**硬编码的 `POSSIBLE_TEST_TARGETS` 和 `ENTELECHY_TEST_LIBS`。

**改为**：通过 CMake 全局 property `ENTELECHY_ENABLED_MODULES` 自动发现所有 `*Tests` target，收集 `$<TARGET_OBJECTS:*Tests>`，并自动链接所有非测试模块。

```cmake
cmake_minimum_required(VERSION 3.20)

entelechy_get_enabled_modules(ALL_MODULES)

set(test_objects "")
set(test_targets "")
foreach(target ${ALL_MODULES})
    if(target MATCHES "Tests$")
        list(APPEND test_objects "$<TARGET_OBJECTS:${target}>")
        list(APPEND test_targets ${target})
    endif()
endforeach()

# 收集所有非测试库
set(test_libs "")
foreach(target ${ALL_MODULES})
    if(NOT target MATCHES "Tests$" AND NOT target STREQUAL "Entelechy")
        list(APPEND test_libs ${target})
    endif()
endforeach()

entelechy_module(
    NAME EntelechyTests
    TYPE EXECUTABLE
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/main.cpp
        ${test_objects}
    PRIVATE_DEPS ${test_libs} ${test_targets}
    NO_TESTS
)
```

### 1.8 修改 `scripts/build/build.py`

删除 `launch/generator.py` 的调用（第241行）：
```python
# 删除这行
run([sys.executable, "launch/generator.py", "--config", config_path])
```

因为 CMake configure 阶段自己会处理模块发现和 `main.cpp` 生成。

---

## Phase 2：清理模块边界（去掉 `..` 暴露）

### 目标
逐步去掉 `${CMAKE_CURRENT_LIST_DIR}/..` 的 `PUBLIC` 暴露，改为仅暴露模块自身目录。

### 问题分析
当前代码中有两种跨模块 include 风格：
1. **裸文件名**：`#include "world.h"`（需要 `_engine/source/core/` 在路径中）
2. **模块名前缀**：`#include "core/app.h"`（需要 `_engine/source/` 在路径中）

风格2依赖于 `..` 暴露。去掉 `..` 后，风格2的 include 会失效。

### 实施步骤
1. 在 `entelechy_module()` 宏中，将 `${CMAKE_CURRENT_LIST_DIR}/..` 从 `PUBLIC` 移到 `PRIVATE`（仅供模块内部使用）。
2. 扫描所有跨模块 include（如 `#include "log/log_macros.h"`、`#include "core/atom_registry.h"`），改为裸文件名风格（`#include "log_macros.h"`、`#include "atom_registry.h"`），或改为通过依赖模块的 include 路径访问。
3. 对 `main.cpp` 中大量硬编码的 include 做同样处理。

---

## Phase 3：引入 public/private 目录边界

### 目标
实现 UE 风格的模块 API 边界控制。

### 目录结构改造（以 `math` 为例）

调整前：
```
_engine/source/math/
  math_lib.cpp
  math_lib.h
  vec.h
  mat4.h
  tests/
```

调整后：
```
_engine/source/math/
  public/
    math_lib.h
    vec.h
    mat4.h
  private/
    math_lib.cpp
  tests/
```

### 宏更新
`entelechy_module()` 改为：
```cmake
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/public")
    target_include_directories(${MOD_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/public)
else()
    target_include_directories(${MOD_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
endif()
target_include_directories(${MOD_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/private)
```

所有跨模块 include 改为：
```cpp
#include "MathLib/mat4.h"   // 或保留模块目录名前缀
```

---

## 文件改动清单（Phase 1）

| 文件 | 操作 | 说明 |
|------|------|------|
| `cmake/EntelechyModule.cmake` | **新增** | 统一模块宏 |
| `CMakeLists.txt` | **重写** | 去掉代理层，直接 add_subdirectory |
| `launch/templates/main.cpp` | **重命名** → `main.cpp.in` | 供 CMake configure_file 使用 |
| `launch/generator.py` | **简化** | 删除代理生成和硬编码映射 |
| `scripts/build/build.py` | **修改** | 删除 generator.py 调用 |
| `launch/cmake_projects.json` | **修改** | 为 core/bridge/window/imgui/runtime 添加 `init_function` |
| `_game/cmake_projects.json` | **修改** | 为 runtime 添加 `init_function` |
| `_engine/source/*/CMakeLists.txt`（15 个） | **修改** | 改为调用 `entelechy_module()` |
| `_game/source/runtime/CMakeLists.txt` | **修改** | 同上 |
| `AGENTS-BUILD.md` | **修改** | 删除代理层/硬编码说明，更新添加模块流程 |
| `build/sourcetree/` | **忽略/删除** | 旧代理目录，不再生成 |
| `launch/templates/CMakeLists.txt` | **可删除** | 代理根模板不再需要 |
| `launch/templates/cmake/json_parser.cmake` | **可删除** | 不再需要 |

---

## 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 宏引入后编译失败 | 高 | Phase 1 保留 `..` 暴露；逐个模块验证 |
| include 路径变更导致头文件找不到 | 高 | Phase 1 不改 include 风格，Phase 2 逐步迁移 |
| `file(GLOB)` vs 手动列表 | 中 | **保持手动列表**，不引入 GLOB |
| 外部 Conan 包（glad/glfw3/imgui）引用 | 中 | 全部 `find_package` 提到顶层，通过 `target_link_libraries` 传递 |
| `BaseLib` 当前暴露了 `../memory` | 低 | 重构时通过 `PUBLIC_DEPS MemoryLib` 替代 |
| generator.py 被完全移除后，旧流程依赖 | 低 | 在 `build.py` 中删除对应调用即可 |

---

## 验证方式

1. **配置验证**：
   ```bash
   python scripts/build/build.py --debug
   ```
   应成功运行 CMake configure，无错误，输出中显示所有模块被注册。

2. **构建验证**：
   ```bash
   python scripts/build/build.py --debug --build
   ```
   应成功编译 `Entelechy.exe` 和 `EntelechyTests.exe`。

3. **运行验证**：
   ```bash
   ./build/bin/Debug/Entelechy.exe
   ```
   窗口应正常出现，ImGui 面板、立方体渲染正常。

4. **新增模块验证**：
   创建 `_engine/source/newmod/`，添加 `CMakeLists.txt` 调用 `entelechy_module(NAME NewModLib ...)`，在 `launch/cmake_projects.json` 中注册，重新 configure。应自动包含，无需修改 generator.py。
