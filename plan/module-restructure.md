# 模块重构计划（UE 风格分层）

## Context

当前模块体系与 UE 的 `Core → Engine` 分层存在显著差距，主要体现为**模块粒度过碎、边界倒置、核心层分裂**。

### 问题 1：Foundation 层分裂为三个模块

UE 的 **Core** 模块是一个内聚的"标准库"：基础类型、容器、字符串、数学、内存分配器全部在一个模块里。当前项目把它们拆成了：

- `BaseLib`：基础类型 + 容器 + 字符串
- `MemoryLib`：分配器实现
- `MathLib`：纯数学

这三者之间几乎没有独立的演化边界。`MemoryLib` 只依赖 `BaseLib`，`MathLib` 也只依赖 `BaseLib`，拆分带来的不是灵活性，而是**多余的 CMake 维护成本和循环依赖风险**。

### 问题 2：CoreLib（ECS）被"大杂烩"污染，且名不副实

UE 的 `Core` 是标准库，`Engine` 才是运行时。当前项目的 `CoreLib` 同时承载了：

- ECS 基础设施（World、EntityRegistry、Query、Scheduler）
- 运行时框架（App、Plugin）
- 资源与序列化（Prefab、SceneSerializer）
- 反射与工具（TypeRegistry、AtomRegistry、inspector_reflection）

这导致任何只想要"纯 ECS"的消费者（如 headless server、单元测试）都被迫拉入 Prefab 和序列化。

### 问题 3：MathLib 被 ECS 污染

`MathLib` 中混杂了 ECS 组件和系统（`transform_component`、`transform_propagation_system`、`hierarchy`），导致纯数学库反向依赖 ECS。

---

## 目标

参照 UE 的 `Core → Engine` 分层，建立以下架构：

1. **CoreLib** = 原 `BaseLib` + `MemoryLib` + `MathLib`（纯数学）
   - 对标 UE `Core` 模块：标准库层，零外部引擎依赖
2. **EcsLib** = 原 `CoreLib`（改名）+ `MathLib`（ECS 内容）
   - 对标 UE `Engine` 模块的核心运行时：ECS 数据层 + 调度 + App/Plugin
3. 删除 `MemoryLib` 和 `MathLib` 两个独立模块
4. 建立清晰的无环依赖链

---

## 新模块架构总览

| 层级 | 模块 | 来源 | 职责 | 依赖 |
|------|------|------|------|------|
| **L0 标准库** | **CoreLib** | BaseLib + MemoryLib + MathLib（纯数学） | 基础类型、容器、字符串、路径、内存分配器、数学库 | 零依赖 |
| **L1 平台与工具** | ThreadPoolLib | 现有 | 线程池 | CoreLib |
| | VFSLib | 现有 | 虚拟文件系统 | CoreLib |
| | LogLib | 现有 | 日志门面 + 多后端 | CoreLib |
| | WindowLib | 现有 | GLFW 窗口 + 输入 | CoreLib + LogLib + glfw |
| **L2 ECS 核心** | **EcsLib** | 原 CoreLib + MathLib（ECS 部分） | World、Query、Scheduler、App、Plugin、Prefab、序列化、反射、事件、变换系统 | CoreLib |
| **L3 资产** | AssetLib | 现有 | AssetHandle、AssetServer | CoreLib + VFSLib |
| **L4 渲染** | RenderLib | 现有 | OpenGL 后端、渲染管线、RHI | CoreLib + EcsLib + WindowLib + LogLib + AssetLib + glad |
| **L5 系统层** | MotorLib | 现有 | MovementSystem、TransformPropagationSystem | EcsLib |
| **L6 工具与桥接** | ImGuiLib | 现有 | ImGui 集成 + 面板 | CoreLib + EcsLib + WindowLib + RenderLib + LogLib + 第三方库 |
| | BridgeLib | 现有 | AgentBridge、ToolRegistry | EcsLib + MotorLib + LogLib |
| **L7 游戏层** | RuntimeLib | _game/ | GameRuntime、GamePlugin | EcsLib + BridgeLib + ... |

### 目标依赖图

```
CoreLib（Foundation） ← 零依赖
  ↑
  ├── ThreadPoolLib
  ├── VFSLib
  ├── LogLib
  └── WindowLib ← glfw
       ↑
       ├── RenderLib ← glad + AssetLib
       └── ImGuiLib ← glad + glfw + imgui
            ↑
EcsLib（ECS + Runtime） ← CoreLib
  ↑
  ├── AssetLib ← VFSLib
  ├── MotorLib
  ├── BridgeLib ← MotorLib + LogLib
  └── RuntimeLib（_game/） ← BridgeLib + RenderLib + ImGuiLib + ...
```

---

## 目录与库名映射

| 当前目录 | 当前库名 | 目标目录 | 目标库名 | 说明 |
|---------|---------|---------|---------|------|
| `_engine/source/base/` | `BaseLib` | 保留 `base/` | **CoreLib** | Foundation 层 |
| `_engine/source/memory/` | `MemoryLib` | **删除** | — | 合并入 `base/` |
| `_engine/source/math/` | `MathLib` | **删除** | — | 纯数学入 `base/math/`，ECS 内容入 `core/` |
| `_engine/source/core/` | `CoreLib` | 保留 `core/` | **EcsLib** | ECS + Runtime |

> **为什么不改目录名？** `base/` → `core/` 和 `core/` → `ecs/` 的物理重命名会引发全局 include 路径地震。本次重构先完成**库名对齐**和**文件移动**，目录重命名放到后续的构建体系重构（Plan 1）中顺势处理。

---

## 执行步骤

### Step 1：合并 MemoryLib 入 base/（并入 CoreLib）

**操作**：删除 `memory/` 目录，文件迁入 `base/memory/`

| 文件 | 当前位置 | 目标位置 |
|------|---------|---------|
| `allocator.h` | `memory/allocator.h` | `base/allocator.h` | 分配器接口放根目录，容器裸引 `#include "allocator.h"` 不变 |
| `iallocator.h` | `memory/iallocator.h` | `base/iallocator.h` | 同上 |
| `frame_arena.h/.cpp` | `memory/frame_arena.*` | `base/memory/frame_arena.*` | 具体实现放子目录 |
| `object_pool.h` | `memory/object_pool.h` | `base/memory/object_pool.h` | 同上 |
| `debug_allocator_wrapper.h/.cpp` | `memory/debug_allocator_wrapper.*` | `base/memory/debug_allocator_wrapper.*` | 同上 |

**CMake 修改**：
- 删除 `memory/CMakeLists.txt`
- `base/CMakeLists.txt`：
  - 库名 `BaseLib` → `CoreLib`
  - 删除 `../memory` 的 include 暴露
  - 新增 `base/memory/` 下的所有源文件
  - 删除 `target_link_libraries`（CoreLib 零依赖）

**测试迁移**：
- `memory/tests/test_memory.cpp` → `base/tests/test_memory.cpp`
- `memory/tests/CMakeLists.txt`（`MemoryTests` 目标）→ 删除
- `base/tests/CMakeLists.txt`：
  - 目标名 `BaseTests` → `CoreTests`
  - 新增 `test_memory.cpp` 到 `CoreTests` 源文件列表
  - `target_link_libraries` 中 `BaseLib` → `CoreLib`

### Step 2：拆分 MathLib，分别并入 CoreLib 与 EcsLib

**操作**：删除 `math/` 目录，纯数学文件迁入 `base/math/`，ECS 文件迁入 `core/`

| 文件 | 当前位置 | 目标位置 | 说明 |
|------|---------|---------|------|
| `math_lib.h/.cpp` | `math/math_lib.*` | `base/math/math_lib.*` | 纯数学 |
| `math_config.h` | `math/math_config.h` | `base/math/math_config.h` | 纯数学 |
| `align.h` | `math/align.h` | `base/math/align.h` | 纯数学 |
| `vec.h` | `math/vec.h` | `base/math/vec.h` | 纯数学 |
| `quat.h` | `math/quat.h` | `base/math/quat.h` | 纯数学 |
| `mat4.h` | `math/mat4.h` | `base/math/mat4.h` | 纯数学 |
| `aabb.h` | `math/aabb.h` | `base/math/aabb.h` | 纯数学 |
| `random.h` | `math/random.h` | `base/math/random.h` | 纯数学 |
| `simd.h` | `math/simd.h` | `base/math/simd.h` | 纯数学 |
| `half.h` | `math/half.h` | `base/math/half.h` | 纯数学 |
| `ray.h` | `math/ray.h` | `base/math/ray.h` | 纯数学 |
| `frustum.h` | `math/frustum.h` | `base/math/frustum.h` | 纯数学 |
| `transform_component.h` | `math/transform_component.h` | `core/transform_component.h` | ECS 组件 |
| `transform_propagation_system.h/.cpp` | `math/transform_propagation_system.*` | `core/transform_propagation_system.*` | ECS 系统 |
| `hierarchy.h` | `math/hierarchy.h` | `core/hierarchy.h` | ECS 组件 |

**CMake 修改**：
- 删除 `math/CMakeLists.txt`
- `base/CMakeLists.txt`（CoreLib）：
  - 新增 `base/math/` 下的所有源文件
  - 暴露 `base/math/` 为 include 路径（或直接通过 `base/` 根目录暴露）
- `core/CMakeLists.txt`（EcsLib）：
  - 库名 `CoreLib` → `EcsLib`
  - 新增移入的 3 个 ECS 文件到源文件列表
  - `target_link_libraries`：`MemoryLib` 引用删除，`BaseLib` → `CoreLib`

**测试迁移**：
- `math/tests/` 下的所有 `test_*.cpp` → `base/tests/`
- `math/tests/CMakeLists.txt`（`MathTests` 目标）→ 删除
- `base/tests/CMakeLists.txt`（CoreTests）：
  - 新增所有数学测试源文件

### Step 3：全局依赖链修正

所有 `CMakeLists.txt` 中的模块名替换：

| 旧引用 | 新引用 | 涉及模块 |
|--------|--------|---------|
| `BaseLib` | `CoreLib` | TestFrameworkLib, AssetLib, VFSLib, ThreadPoolLib, TestRunner |
| `CoreLib`（原 ECS） | `EcsLib` | LogLib, WindowLib, MotorLib, BridgeLib, ImGuiLib, RuntimeLib, TestRunner |
| `MemoryLib` | 删除 | CoreLib(旧), EcsLib(旧) |
| `MathLib` | 删除 | MotorLib(旧), BridgeLib(旧), TestRunner |

**特殊处理**：

| 模块 | 修改内容 |
|------|---------|
| **LogLib** | `target_link_libraries` 中 `CoreLib`（原）→ `CoreLib`（新 Foundation）。日志是底层设施，不应依赖 ECS。若编译报错（说明 LogLib 实际使用了 ECS 类型），则暂时改为 `EcsLib`，并记 `TODO.md` 债务。 |
| **WindowLib** | `target_link_libraries` 中 `CoreLib`（原）→ `CoreLib`（新）。WindowLib 是平台抽象，不应依赖 ECS。若 `input_queue.cpp` 使用了 `KeyboardEvent`（ECS 事件），需确认是否可降级为原始输入结构。若不能，暂时保留 `EcsLib`。 |
| **RenderLib** | 当前不链接 ECS，但拥有 `ExtractRenderablesSystem.cpp` 等 ECS 系统文件。`target_link_libraries` 需显式添加 `EcsLib` 和 `CoreLib`。 |
| **ImGuiLib** | 当前链接 `CoreLib`（原）。改为链接 `CoreLib`（新）+ `EcsLib`（`imgui_atom_registry` 需要 ECS 反射）。 |
| **MotorLib** | `target_link_libraries` 中 `CoreLib` → `EcsLib`。MovementSystem 消费 World 数据。 |
| **BridgeLib** | `target_link_libraries` 中 `CoreLib` → `EcsLib`。AgentBridge 查询/修改 World。 |
| **RuntimeLib** | `target_link_libraries` 中 `CoreLib` → `EcsLib`。GamePlugin 注册 ECS 系统。 |
| **TestRunner** | `POSSIBLE_TEST_TARGETS` 中：`BaseTests` → `CoreTests`，`CoreTests` → `EcsTests`，删除 `MathTests` / `MemoryTests`。`ENTELECHY_TEST_LIBS` 中：`BaseLib` → `CoreLib`，`CoreLib` → `EcsLib`，删除 `MathLib` / `MemoryLib`。 |

### Step 4：全局 Include 路径替换

跨模块引用中涉及目录路径的 include 需要修正：

| 旧 include | 新 include | 原因 |
|-----------|-----------|------|
| `#include "memory/allocator.h"` | `#include "memory/allocator.h"` | 路径不变，目录仍在 `base/memory/` |
| `#include "allocator.h"`（裸引） | `#include "memory/allocator.h"` | 若原通过 `../memory` hack 引入，现在需显式路径 |
| `#include "math/vec.h"` | `#include "math/vec.h"` | 路径不变，目录仍在 `base/math/` |
| `#include "math/transform_component.h"` | `#include "core/transform_component.h"` | 文件已移到 `core/` |
| `#include "math/hierarchy.h"` | `#include "core/hierarchy.h"` | 文件已移到 `core/` |
| `#include "math/transform_propagation_system.h"` | `#include "core/transform_propagation_system.h"` | 文件已移到 `core/` |
| `#include "base/foundation_types.h"` | `#include "base/foundation_types.h"` | 路径不变 |

> 由于目录名未改，`base/xxx.h` 和 `core/xxx.h` 的跨模块 include 大部分保持不变。只需处理 `math/` 下 ECS 文件的迁移。

### Step 5：AGENTS.md 同步

按项目规则，模块增删和职责变更必须同步更新 `AGENTS.md`：

- 删除 `memory/AGENTS.md`
- 删除 `math/AGENTS.md`
- 重写 `base/AGENTS.md`：职责改为"Foundation 标准库层"，包含原 BaseLib + MemoryLib + MathLib 的说明
- 重写 `core/AGENTS.md`：职责改为"ECS 运行时 + 应用框架"，模块名改为 EcsLib
- 更新所有引用 `BaseLib` / `MemoryLib` / `MathLib` / `CoreLib`（旧含义）的模块级 AGENTS.md
- 更新根目录 `AGENTS-BUILD.md` 中的模块列表（如果存在）

---

## 文件改动清单

### 删除

| 路径 | 说明 |
|------|------|
| `_engine/source/memory/CMakeLists.txt` | MemoryLib 删除 |
| `_engine/source/memory/tests/CMakeLists.txt` | MemoryTests 删除 |
| `_engine/source/math/CMakeLists.txt` | MathLib 删除 |
| `_engine/source/math/tests/CMakeLists.txt` | MathTests 删除 |
| `_engine/source/memory/AGENTS.md` | 文档同步 |
| `_engine/source/math/AGENTS.md` | 文档同步 |

### 移动（物理迁移）

| 源路径 | 目标路径 |
|--------|---------|
| `memory/allocator.h` | `base/allocator.h` |
| `memory/iallocator.h` | `base/iallocator.h` |
| `memory/frame_arena.h` | `base/memory/frame_arena.h` |
| `memory/frame_arena.cpp` | `base/memory/frame_arena.cpp` |
| `memory/object_pool.h` | `base/memory/object_pool.h` |
| `memory/debug_allocator_wrapper.h` | `base/memory/debug_allocator_wrapper.h` |
| `memory/debug_allocator_wrapper.cpp` | `base/memory/debug_allocator_wrapper.cpp` |
| `memory/tests/test_memory.cpp` | `base/tests/test_memory.cpp` |
| `math/math_lib.h` | `base/math/math_lib.h` |
| `math/math_lib.cpp` | `base/math/math_lib.cpp` |
| `math/math_config.h` | `base/math/math_config.h` |
| `math/align.h` | `base/math/align.h` |
| `math/vec.h` | `base/math/vec.h` |
| `math/quat.h` | `base/math/quat.h` |
| `math/mat4.h` | `base/math/mat4.h` |
| `math/aabb.h` | `base/math/aabb.h` |
| `math/random.h` | `base/math/random.h` |
| `math/simd.h` | `base/math/simd.h` |
| `math/half.h` | `base/math/half.h` |
| `math/ray.h` | `base/math/ray.h` |
| `math/frustum.h` | `base/math/frustum.h` |
| `math/tests/test_*.cpp` | `base/tests/test_*.cpp` |
| `math/transform_component.h` | `core/transform_component.h` |
| `math/transform_propagation_system.h` | `core/transform_propagation_system.h` |
| `math/transform_propagation_system.cpp` | `core/transform_propagation_system.cpp` |
| `math/hierarchy.h` | `core/hierarchy.h` |

### 修改（CMake + 代码）

| 文件 | 修改内容 |
|------|---------|
| `base/CMakeLists.txt` | 库名 `BaseLib` → `CoreLib`；删除 `../memory` include；添加 memory/ 和 math/ 源文件；删除 link_libraries |
| `base/tests/CMakeLists.txt` | 目标名 `BaseTests` → `CoreTests`；添加数学和内存测试源文件；link `CoreLib` |
| `core/CMakeLists.txt` | 库名 `CoreLib` → `EcsLib`；添加移入的 3 个 ECS 文件；`BaseLib` → `CoreLib`；删除 `MemoryLib` |
| `core/tests/CMakeLists.txt` | 目标名 `CoreTests` → `EcsTests`；link `EcsLib` |
| `log/CMakeLists.txt` | `CoreLib` → `CoreLib`（若纯日志）或 `EcsLib`（若用 ECS） |
| `window/CMakeLists.txt` | `CoreLib` → `CoreLib`（若纯窗口）或 `EcsLib`（若用 ECS 事件） |
| `render/CMakeLists.txt` | 显式添加 `CoreLib` + `EcsLib` |
| `motor/CMakeLists.txt` | `CoreLib` → `EcsLib`；删除 `MathLib`（数学已在 CoreLib） |
| `bridge/CMakeLists.txt` | `CoreLib` → `EcsLib`；删除 `MathLib` |
| `imgui/CMakeLists.txt` | `CoreLib`（原）→ `CoreLib`（新）+ `EcsLib` |
| `asset/CMakeLists.txt` | `BaseLib` → `CoreLib` |
| `vfs/CMakeLists.txt` | `BaseLib` → `CoreLib` |
| `thread_pool/CMakeLists.txt` | `BaseLib` → `CoreLib` |
| `test/CMakeLists.txt` | `BaseLib` → `CoreLib` |
| `test_runner/CMakeLists.txt` | 测试目标重命名 + 库名替换（见 Step 3） |
| `_game/source/runtime/CMakeLists.txt` | `CoreLib` → `EcsLib` |
| 所有 `.cpp` / `.h` | `#include "math/transform_component.h"` → `#include "core/transform_component.h"` 等 |

---

## 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| `CoreLib` 目标名冲突 | 高 | 原 `BaseTests` 改 `CoreTests`、原 `CoreTests` 改 `EcsTests`，test_runner 列表需同步。先在 `test_runner/CMakeLists.txt` 中完成重命名再构建。 |
| 数学测试合并后编译失败 | 中 | `math/tests/` 使用了 `MathLib` 的 include 路径和宏（如 `ENTELECHY_MATH_STRICT_FP`）。合并到 `base/tests/` 后，需确保这些编译选项在 `CoreLib`（或测试目标）中保留。 |
| RenderLib 补链后暴露隐藏依赖 | 中 | RenderLib 之前未显式链接 ECS，但代码中已使用。补链后如果存在**私有** include 的 ECS 头文件，可能需要把某些 include 改为 public。 |
| LogLib / WindowLib 降级后出现未定义符号 | 中 | 如果它们实际使用了原 CoreLib 的 ECS 类型（如 `KeyboardEvent`、`TypeRegistry`），降级为 `CoreLib` 会导致编译失败。**缓解**：先全部改为链接 `EcsLib` 保证编译通过，再逐个尝试降级，失败的保持 `EcsLib`。 |
| include 路径遗漏 | 高 | 移动 `math/` 的 ECS 文件后，全局搜索 `#include "math/transform` 和 `#include "math/hierarchy`，确保全部替换。 |

---

## 执行顺序

1. **物理移动文件**：按"文件改动清单"完成所有文件迁移
2. **重写 CMakeLists.txt**：按 Step 1~3 逐个模块修正库名和依赖链
3. **全局搜索替换 include**：`math/transform_component.h` / `math/hierarchy.h` / `math/transform_propagation_system.h`
4. **同步 AGENTS.md**：更新模块职责和入口说明
5. **构建验证**：
   ```bash
   python scripts/build/build.py --debug --build
   ```
6. **运行时验证**：
   ```bash
   ./build/bin/Debug/Entelechy.exe
   ```
   确认窗口、立方体、ImGui 面板、层级变换正常。

---

## 验证方式

1. **编译验证**：`python scripts/build/build.py --debug --build` 应成功编译所有模块，零链接错误。
2. **目标存在性验证**：
   - `build/lib/Debug/CoreLib.lib` 存在（原 BaseLib + MemoryLib + MathLib 纯数学）
   - `build/lib/Debug/EcsLib.lib` 存在（原 CoreLib + MathLib ECS 内容）
   - `build/bin/Debug/Entelechy.exe` 存在
3. **依赖方向验证**（可用 `cmake --graphviz` 或脚本抽查）：
   - `CoreLib` 零引擎模块依赖
   - `EcsLib` 只依赖 `CoreLib`
   - `MathLib` / `MemoryLib` 目标不存在
4. **运行时验证**：窗口正常出现，立方体渲染正常，ImGui ECS Inspector 面板正常，父子层级变换正确。
