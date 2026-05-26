# 构建系统规范

## 添加新模块

以 `_engine/source/math/` 为例：

### 约束清单

1. **源码路径**：`CMakeLists.txt` 中必须使用 `${CMAKE_CURRENT_LIST_DIR}`，禁止相对路径。
   ```cmake
   add_library(MathLib STATIC ${CMAKE_CURRENT_LIST_DIR}/math.cpp)
   ```
2. **注册模块**：在 `_engine/cmake_projects.json`（或 `_game/cmake_projects.json`）的 `modules` 数组中添加条目：
   ```json
   { "name": "math", "path": "source/math", "enabled": true }
   ```
3. **链接注册**（如模块需被可执行文件链接）：在 `launch/generator.py` 的 `link_libs` 区域按名称关键字追加：
   ```python
   elif 'math' in m['name'].lower():
       link_libs.append('MathLib')
   ```
4. **初始化调用**（如模块需在 `main()` 中初始化）：在同文件的 `generate_main_cpp` 中追加前向声明和调用。

## 构建陷阱

- ❌ **不要直接修改 `build/sourcetree/`**。该目录由 `generator.py` 生成，每次构建都会被覆盖。
- ❌ **不要在 `CMakeLists.txt` 中使用相对路径**。代理 include 时相对路径会失效，必须使用 `${CMAKE_CURRENT_LIST_DIR}`。
