# Entelechy — 构建系统与模块添加指南

## 构建系统：轻量版 Source Forest

本项目采用类似大型商业引擎的 **Source Forest** 架构。

### 为什么不是简单 CMake？

- 引擎代码（`_engine/`）与游戏代码（`_game/`）物理分离，模拟多仓库
- 通过 JSON 配置动态决定本次构建包含哪些模块
- CMake 实际面向 `build/sourcetree/` 下的**代理源码树**构建

### 构建入口

**跨平台（推荐）**
```bash
# 完整构建（引擎 + 游戏运行时）
python scripts/build.py

# 仅构建引擎核心
python scripts/build.py --config configs/engine_only.json

# Debug 构建（Agent 改动后的默认测试配置）
python scripts/build.py --debug

# Release 构建
python scripts/build.py --release
```

**Windows**
```batch
scripts/build/build.bat
scripts/build/build.bat --config configs/engine_only.json
```

**macOS / Linux**
```bash
./scripts/build/build.sh
./scripts/build/build.sh --config configs/engine_only.json
```

构建产物：
- 可执行文件 → `build/bin/`
- 静态库 → `build/lib/`

### 构建流程

```
scripts/build/build.py
  → launch/generator.py（读取 JSON 配置）
  → 生成 build/sourcetree/（代理 CMakeLists.txt + main.cpp）
  → cmake -S . -B build
  → cmake --build build
```

**不要手动编辑 `build/sourcetree/` 下的任何文件**，它们会被 `generator.py` 覆盖。

---

## 如何添加新模块

以在 `_engine/source/` 下新增 `math` 模块为例：

### 1. 创建模块目录和源码

```
_engine/source/math/
├── math.cpp
├── math.h
└── CMakeLists.txt
```

### 2. 编写 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)

add_library(MathLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/math.cpp
)

target_include_directories(MathLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(MathLib PUBLIC cxx_std_20)
```

**关键**：源码路径必须使用 `${CMAKE_CURRENT_LIST_DIR}`，因为真实文件会被代理 include。

### 3. 注册模块

编辑 `_engine/cmake_projects.json`：

```json
{
    "modules": [
        ...,
        {
            "name": "math",
            "path": "source/math",
            "enabled": true
        }
    ]
}
```

### 4. 重新构建

```bash
python scripts/build.py
```

---

## 添加新库到链接的约定

如果新模块需要被最终可执行文件链接，还需要在 `launch/generator.py` 中注册库名映射。

搜索 `link_libs = []` 所在区域，按名称关键字追加：

```python
elif 'math' in m['name'].lower():
    link_libs.append('MathLib')
```

同时，如果需要在 `main.cpp` 中调用初始化函数，也在同文件的 `generate_main_cpp` 中追加前向声明和调用。

---

## 构建相关陷阱

### ❌ 不要直接改 `build/sourcetree/`

`sourcetree/` 是生成目录，每次运行 `scripts/build/build.bat` 都会被覆盖。所有持久化修改应该发生在：
- `_engine/source/...`
- `_game/source/...`
- `launch/templates/...`
- `configs/...`

### ❌ 不要在模块 CMakeLists.txt 里写相对路径

错误：
```cmake
add_library(MathLib STATIC math.cpp)  # 当通过代理 include 时，这个相对路径会失效
```

正确：
```cmake
add_library(MathLib STATIC ${CMAKE_CURRENT_LIST_DIR}/math.cpp)
```

---

## VS Code IntelliSense 配置

本项目通过 `compile_commands.json` 自动为 VS Code C++ 扩展提供 Conan 引入包的 include 路径，**无需手动维护 `includePath`**。

- `scripts/build/build.py` 已启用 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
- `.vscode/c_cpp_properties.json` 已配置 `compileCommands` 指向 `${workspaceFolder}/build/compile_commands.json`

**以后引入任何 Conan 新包时**，只需重新运行 `scripts/build/build.py`（或 `scripts/build/build.sh` / `scripts/build/build.bat`），`compile_commands.json` 会自动更新，IntelliSense 即刻识别新包头文件。
