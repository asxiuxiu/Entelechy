# Entelechy

渐进式自研游戏引擎，从最小可运行 ECS 骨架逐步演进到工业级引擎。

## 快速开始

```batch
REM 完整构建（引擎 + 游戏运行时）
scripts\build\build.bat

REM 仅构建引擎核心
scripts\build\build.bat --config configs\engine_only.json
```

构建产物位于 `build/bin/` 和 `build/lib/`。

## 工程架构：轻量版 Source Forest

本项目采用**轻量版 Source Forest** 构建架构，参考了笔记中的《构建系统演进：从单文件到游戏引擎》与 `workspace/build_system_demo` 的设计。

### 为什么用 Source Forest？

- **源码分散**：引擎代码（`_engine/`）与游戏代码（`_game/`）物理分离，模拟多仓库协作
- **模块按需组装**：通过 JSON 配置决定本次构建包含哪些模块
- **CMake 工程化**：每个模块独立静态库，符合大型引擎的 target 组织方式
- **可扩展**：未来插件（`_plugin/`）或独立仓库可无缝接入

### 目录结构

```
Entelechy/
├── README.md
├── plans/                      # 路线图与规划
│   └── SelfGameEngine-Roadmap.md
├── TODO.md                     # 技术债务追踪
├── scripts/                    # 构建与工具脚本
│   ├── build/                  # 构建系统
│   │   ├── build.py            # 跨平台主构建入口
│   │   ├── build.bat           # Windows 构建包装
│   │   └── build.sh            # macOS / Linux 构建包装
│   └── tools/                  # 开发工具
│       ├── clean.py            # 跨平台清理脚本
│       ├── clean.bat           # Windows 清理包装
│       └── clean.sh            # macOS / Linux 清理包装
├── CMakeLists.txt              # 根 CMake 入口
├── configs/                    # 构建配置（JSON 驱动）
│   ├── full_build.json         # 完整构建
│   └── engine_only.json        # 仅引擎核心
├── launch/                     # 构建系统工具链
│   ├── generator.py            # Source Forest 生成器
│   ├── cmake_projects.json     # 引擎模块配置
│   └── templates/              # CMake / main.cpp 模板
│       ├── CMakeLists.txt
│       ├── proxy_cmakelists.txt.in
│       ├── main.cpp
│       └── cmake/
│           └── json_parser.cmake
├── _engine/                    # 引擎核心源码（模拟独立仓库）
│   ├── cmake_projects.json
│   └── source/
│       ├── core/               # ECS 数据层：Entity、World、组件定义
│       ├── system/             # System、Scheduler
│       └── bridge/             # AgentBridge（AI 工具接口）
├── _game/                      # 游戏逻辑层（模拟独立仓库）
│   ├── cmake_projects.json
│   └── source/
│       └── runtime/            # 游戏入口、运行时逻辑
└── build/                      # 构建输出
    └── sourcetree/             # 生成的代理源码树（由 generator.py 产出）
```

### 构建流程

```
scripts/build/build.bat
    │
    ▼
launch/generator.py
    │ 读取 configs/full_build.json
    │ 读取 _engine/cmake_projects.json
    │ 读取 _game/cmake_projects.json
    │
    ▼
build/sourcetree/             ← 代理源码树
    ├── CMakeLists.txt        ← 根入口（含最终 executable）
    ├── _engine/source/core/CMakeLists.txt   ← 代理（include 真实目录）
    ├── _engine/source/system/CMakeLists.txt ← 代理
    ├── _engine/source/bridge/CMakeLists.txt ← 代理
    ├── _game/source/runtime/CMakeLists.txt  ← 代理
    └── main.cpp              ← 根据启用的模块自动生成
    │
    ▼
cmake -S build/sourcetree -B build
    │
    ▼
cmake --build build
    │
    ▼
build/bin/Entelechy.exe
build/lib/CoreLib.lib
build/lib/SystemLib.lib
build/lib/BridgeLib.lib
build/lib/RuntimeLib.lib
```

### 核心原理：代理 CMakeLists.txt

CMake 的 `add_subdirectory()` 只能引用当前目录或子目录。Source Forest 通过在 `build/sourcetree/` 下生成**代理文件**来解决：

```cmake
# build/sourcetree/_engine/source/core/CMakeLists.txt（代理）
set(REAL_SOURCE_PATH "D:/workspace/Entelechy/_engine/source/core")
include(${REAL_SOURCE_PATH}/CMakeLists.txt)
```

真实模块的 CMakeLists.txt 使用 `${CMAKE_CURRENT_LIST_DIR}` 引用自身目录下的源文件，因此可以被任意位置的代理安全包含。

### 添加新模块

1. 在 `_engine/source/` 或 `_game/source/` 下新建目录
2. 编写模块源码 + `CMakeLists.txt`
3. 在对应仓库的 `cmake_projects.json` 中注册模块
4. 重新运行 `scripts/build/build.bat`

示例（新增 `_engine/source/math`）：

```cmake
# _engine/source/math/CMakeLists.txt
add_library(MathLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/math.cpp
)
target_include_directories(MathLib PUBLIC ${CMAKE_CURRENT_LIST_DIR})
```

```json
// _engine/cmake_projects.json
{
    "modules": [
        ...
        {
            "name": "math",
            "path": "source/math",
            "enabled": true
        }
    ]
}
```

## 开发阶段

当前处于 **Phase 0：最小可运行骨架（MVP）**

- ✅ Milestone 0.1：平铺 ECS 数据层（`World`、`Entity`、`Position`、`Velocity`）
- 🔄 Milestone 0.2 ~ 0.4：TypeRegistry、System 调度、AgentBridge 接口（持续迭代中）

详见 `plans/SelfGameEngine-Roadmap.md`。
