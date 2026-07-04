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

首次构建前初始化项目本地环境（创建 `.venv`、安装 `configs/environment.json` 指定的 Conan / CMake / Ninja、初始化项目级 `.conan_home`）：

```bash
python scripts/tools/setup_env.py
```

Windows 上若 Visual Studio 安装在非默认路径，需先复制模板并填写本机路径：

```bash
cp configs/environment.local.json.template configs/environment.local.json
# 编辑 configs/environment.local.json，补充 msvc.search_paths
```

`setup_env.py` 会生成 venv 激活脚本并自动设置 `CONAN_HOME`：

- Windows cmd：`.venv/Scripts/activate_entelechy.bat`
- Windows PowerShell：`.venv/Scripts/Activate_entelechy.ps1`
- Git Bash / WSL：`.venv/Scripts/activate_entelechy`

激活后，当前 shell 中的 `conan` 命令会自然使用项目独立的 `.conan_home`。`build.py` 也会通过 `env_config.py` 自动设置 `CONAN_HOME`，因此不激活也可以直接运行。

```bash
# 默认配置 + 编译
python scripts/build/build.py --debug --build

# Release
python scripts/build/build.py --release --build
```

`build.py` 会优先使用 `.venv/Scripts/conan.exe`（Windows）或 `.venv/bin/conan`（Unix），以及 `.venv` 中安装的 `cmake`，避免受宿主机工具版本影响。

### `--strict-build`（可选）

Windows 上 Conan 中央仓库的预编译包常以 `compiler.version=194`（v144 工具集）发布。当本机实际使用 `compiler.version=193`（v143 工具集）时，Conan 会 fallback 到 193 的兼容包，可能在 Visual Studio Multi-config generator 下产生 `IMPORTED_LOCATION not set...` 等噪音。

`build.py` 现在会按 `configs/environment.json` 中 `msvc.search_paths` 检测本机实际安装的默认工具集（VS 2022 默认 v143 → 193），自动生成匹配的 profile，因此正常构建通常不会再遇到 fallback 问题。若仍希望强制从源码编译 host 依赖以彻底消除兼容性差异，可使用：

```bash
python scripts/build/build.py --debug --build --strict-build
```

> ⚠️ `--strict-build` 使用 `--build=pkg/*` 模式，会**强制重新编译**所有匹配包，即使缓存命中。适合 CI 或首次搭建可复现环境；日常增量开发不建议默认开启。

### `--compile-only`（增量编译）

在已完成 `build.py` 生成+配置后，若只改了源码想快速编译，可使用：

```bash
python scripts/build/build.py --debug --compile-only
```

该命令跳过 Conan install 和 CMake configure，直接调用项目 `.venv` 内的 cmake 执行 `cmake --build build --config Debug --parallel`。Zed / VS Code 的 `Compile: Debug/Release` 任务已默认使用此模式。

---

## 环境配置文件

项目通过两层 JSON 管理构建环境，避免机器相关路径写死在脚本里。

### `configs/environment.json`（进 git）

项目级不变量，所有开发者共享：

```json
{
    "python": { "min_version": "3.12" },
    "conan": {
        "version": "2.30.0",
        "home": ".conan_home"
    },
    "cmake": {
        "version": "3.31.10",
        "install_in_venv": true
    },
    "ninja": {
        "version": "1.13.0",
        "install_in_venv": true
    },
    "msvc": {
        "search_paths": [
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community"
        ]
    }
}
```

- `conan.version` / `cmake.version`：锁定构建工具版本，`setup_env.py` 会安装到 `.venv`。
- `conan.home`：项目级 Conan home 目录，相对项目根。Conan 缓存、profile、settings 全部隔离在此，不污染用户目录下的 `~/.conan2`。
- `cmake.install_in_venv`：为 true 时，把 CMake 作为 Python 包装进 `.venv`，`build.py` 优先使用它。
- `ninja.install_in_venv`：为 true 时，把 Ninja 装到 `.venv`，用于生成 `compile_commands.json` 的 Ninja 配置。
- `msvc.search_paths`：Windows 上搜索 `vcvarsall.bat` 的候选 VS 安装目录。

### `configs/environment.local.json`（gitignore）

机器级覆盖/补充。首次克隆后从模板创建：

```bash
cp configs/environment.local.json.template configs/environment.local.json
```

典型用途是补充非默认 VS 路径：

```json
{
    "msvc": {
        "search_paths": [
            "D:\\visual studio\\vs2022_pro"
        ]
    }
}
```

`environment.local.json` 中的列表会与基础配置**追加合并**；不要在此放项目级版本信息，避免各机器版本漂移。

### `scripts/tools/env_config.py`

读取并合并两层配置的公共模块，`setup_env.py` 和 `build.py` 都会使用。直接运行可查看合并后的最终配置：

```bash
python scripts/tools/env_config.py
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
