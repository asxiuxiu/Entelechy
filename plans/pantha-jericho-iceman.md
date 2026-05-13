# Plan: 基础类型层重构与项目优化

## 目标
参照 Obsidian 笔记《引擎基础类型与平台抽象》，为 Entelechy 建立统一的 Foundation 基础层，并基于新基础层对现有代码进行系统性替换与优化。

## 现状诊断
1. **无引擎基础类型**：直接使用 `<cstdint>` 的 `uint32_t`、`size_t` 等，没有 `u32`、`usize` 等短别名。
2. **无统一平台抽象**：只有 `memory/allocator.h` 等少数文件里零散的 `#ifdef _WIN32`。
3. **无分层断言体系**：`core/type_registry.h` 使用原始 `<cassert>`，Release 中无法保留表达式副作用检查。
4. **无符号导出宏**：当前全静态链接，未来若转 DLL 需要统一接口。
5. **Math 层小缺陷**：`Vec3` 缺少 `operator[]`，影响后续 SIMD/图形代码的通用性。

## 设计方案

### 1. 新建 Foundation 模块（零依赖，最底层）
在 `_engine/source/foundation/` 创建独立模块，作为 **MemoryLib、MathLib 的公共依赖**，符合笔记"基础层零运行时依赖"的要求。

- **`foundation_types.h`** — 核心头文件，包含：
  - 固定宽度整数别名：`u8/u16/u32/u64`, `i8/i16/i32/i64`, `f32/f64`, `usize/isize`
  - 平台检测宏（预置为零 + `#if`）：`PLATFORM_WINDOWS/LINUX/MACOS/ANDROID/IOS/UNKNOWN`, `ARCH_64BIT/ARCH_32BIT`
  - 导出符号宏 `ENGINE_API`：静态链接时为空；DLL 模式时根据 `ENGINE_BUILD_DLL` / `ENGINE_USE_DLL` 切换 `dllexport`/`dllimport`
  - 断言三级体系：`CHECK`（Debug-only 内部不变量）、`VERIFY`（始终执行表达式，Debug 中断点）、`ENSURE`/`ENSURE_MSG`（记录错误并继续）
  - `STATIC_ASSERT` 包装
- **`CMakeLists.txt`** — 注册为 `FoundationLib`（`INTERFACE` 或 `STATIC` 库，仅头文件）
- **`AGENTS.md`** — 按项目规范补充模块文档

### 2. 调整模块依赖链
构建系统修改：
- `launch/cmake_projects.json`：插入 `foundation` 模块（排在最前）
- `memory/CMakeLists.txt`：`target_link_libraries(MemoryLib PUBLIC FoundationLib)`
- `math/CMakeLists.txt`：`target_link_libraries(MathLib PUBLIC FoundationLib)`
- `core/CMakeLists.txt`：无需显式 link，因为 MemoryLib 已传递依赖

### 3. 类型与宏替换（最小侵入原则）
**范围**：`_engine/source/` 下所有使用了裸标准整数类型的引擎文件；`_game/source/` 中若使用了引擎头文件也同步替换。

| 旧写法 | 新写法 | 说明 |
|--------|--------|------|
| `uint32_t` | `u32` | ECS id、mask、generation 等 |
| `int32_t` | `i32` | 极少，遇则替换 |
| `uint64_t` | `u64` | 如需要 |
| `size_t` | `usize` | 引擎数据路径（`std::hash` 内部保持 `std::size_t`） |
| `float` | `f32` | 数学/组件数据中的 `float` |
| `#ifdef _WIN32` | `#if PLATFORM_WINDOWS` | 统一平台判断风格 |
| `assert(...)` | `CHECK(...)` | 内部不变量（如 `index < arraySize`） |
| `#include <cassert>` | 移除 | 统一走 `foundation_types.h` |
| `0xFFFFFFFFu` | 保留或视场景用 `~0u` | 作为常量保留也可接受 |

**重点文件**：
- `core/types.h`：`Entity.id/generation` → `u32`
- `core/type_registry.h`：`ComponentTypeID` → `u32`；`assert(m_nextID < 32)` → `CHECK`；移除 `<cassert>`
- `core/world.h`：`uint32_t` → `u32`；`size_t` → `usize`
- `memory/allocator.h`：`size_t` → `usize`；`#ifdef _WIN32` → `#if PLATFORM_WINDOWS`
- `memory/frame_arena.h`, `dynamic_array.h`, `object_pool.h`：`size_t` → `usize`
- `math/vec.h`：增加 `operator[]`
- `math/align.h`, `random.h`：使用 `u32`/`usize`
- `log/`, `bridge/`, `imgui/` 中涉及引擎类型的文件一并替换

### 4. 基于新基础层的额外优化
- **`Vec3::operator[]`**：提供 `float& operator[](usize i)` 与 `const float& operator[](usize i) const`，方便图形/物理代码索引访问
- **`type_registry.h` 断言语义升级**：`assert(m_nextID < 32)` 属于内部不变量，用 `CHECK`；若未来有带副作用的校验（如文件打开），用 `VERIFY`

### 5. 编码与文档规范
- 所有新建/修改的 `.h/.cpp` 文件保持 **UTF-8 with BOM**
- 注释保持 **英文**
- 完成修改后，**Debug 构建验证**（`python scripts/build/build.py --debug --build`）
- 同步更新根 `AGENTS.md` 目录结构说明
- 同步更新 `core/AGENTS.md`、`memory/AGENTS.md`、`math/AGENTS.md` 中的类型/文件描述

## 实施步骤

1. **创建 Foundation 模块**
   - `_engine/source/foundation/foundation_types.h`
   - `_engine/source/foundation/CMakeLists.txt`
   - `_engine/source/foundation/AGENTS.md`

2. **注册模块到构建系统**
   - `launch/cmake_projects.json`

3. **绑定依赖**
   - `memory/CMakeLists.txt` link FoundationLib
   - `math/CMakeLists.txt` link FoundationLib

4. **核心替换**
   - `core/type_registry.h`（类型 + 断言）
   - `core/types.h`
   - `core/world.h`
   - `memory/allocator.h`（类型 + 平台宏）

5. **扩散替换**
   - `memory/*`, `math/*`, `core/*`, `log/*`, `bridge/*`, `imgui/*` 中所有 `uint32_t`/`size_t`/`float`（引擎语义下）

6. **Math 优化**
   - `math/vec.h` 增加 `operator[]`

7. **文档同步**
   - 根 `AGENTS.md`
   - 相关子模块 `AGENTS.md`

8. **构建验证**
   - `python scripts/build/build.py --debug --build`
   - 若编译失败，按错误文件回修

## 风险评估
| 风险 | 缓解措施 |
|------|----------|
| `usize` 与 `std::size_t` 在 STL 接口中混用导致类型警告 | `std::hash`、`std::vector::size()` 等标准接口保持使用 `std::size_t`，仅在引擎自定义 API 中使用 `usize` |
| `ENGINE_API` 在静态链接下误定义 | 宏中增加 `#elif defined(ENGINE_USE_DLL) #else #define ENGINE_API /* static */` 分支，当前静态构建不注入任何定义，行为与修改前一致 |
| 替换面过广导致编译错误难以定位 | 按模块分批替换：Foundation → Memory → Math → Core → 其余；每批后构建验证 |
| `operator[]` 未做边界检查导致越界 | `operator[]` 使用 `CHECK(i < 3)` 保护（Debug 中断点），Release 零开销 |
