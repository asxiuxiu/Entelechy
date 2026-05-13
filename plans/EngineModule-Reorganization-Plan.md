# Entelechy 引擎模块分包重构计划

> 参考知识库中 UE/chaos 与 Bevy 的分包模式，针对当前 `memory` 容器混杂、`core` 职责膨胀、`system` 边界模糊等问题，提出可落地的改进方案。

---

## 一、当前分包的问题诊断

### 1.1 `memory` —— "内存分配器" 之名，行 "容器库" 之实

当前 `memory/` 下包含：

| 文件 | 实际语义 | 应属模块 |
|------|---------|---------|
| `iallocator.h` / `allocator.h` | 分配器接口与全局分配器 | ✅ memory |
| `debug_allocator_wrapper.h` | 调试装饰器 | ✅ memory |
| `frame_arena.h` / `object_pool.h` | 帧分配器、对象池 | ✅ memory |
| `dynamic_array.h` / `hash_map.h` | 动态数组、哈希表 | ❌ **base/容器** |
| `inline_array.h` / `ring_buffer.h` | 内联数组、无锁环形队列 | ❌ **base/容器** |

**问题**：容器和分配器是两个完全不同的抽象层级。容器是"数据结构"，分配器是"内存策略"。把它们塞进同一个模块，导致：
- `core` 为了用 `HashMap` 必须依赖整个 `memory`
- 未来 `base/` 想定义不依赖堆分配的基础类型时，反而要先依赖 `memory`
- 违背"最小语义单元"原则

### 1.2 `core` —— ECS 心脏 + 字符串工具箱的缝合体

当前 `core/` 下包含：

| 文件 | 实际语义 | 应属模块 |
|------|---------|---------|
| `world.h` / `component_array.h` / `query.h` | ECS 核心数据层 | ✅ core |
| `type_registry.h` / `archetype_chunk.h` | 类型注册、Chunk 存储 | ✅ core |
| `string_id.h` / `small_string.h` / `string_format.h` / `string_intern_pool.h` | 字符串系统 | ❌ **base/字符串** |

**问题**：字符串系统是"基础工具"，不是"ECS 基础设施"。Bevy 的字符串工具在 `bevy_utils`，UE 的字符串在 `base/`。放在 `core` 里会让所有只需要字符串的模块被迫依赖 ECS。

### 1.3 `system` —— 调度器基础设施与 Gameplay 系统混居

当前 `system/` 下包含：

| 文件 | 实际语义 | 应属模块 |
|------|---------|---------|
| `scheduler.h` | System 调度基础设施 | ✅ system/scheduler |
| `movement_system.cpp` / `transform_propagation_system.cpp` | 具体 Gameplay System | ❌ **gameplay/ 或 _game/** |

**问题**：`TransformPropagationSystem` 和 `MovementSystem` 是**业务逻辑**，不是引擎基础设施。它们依赖 `math` 的 `TransformComponent`，放在引擎核心层会造成：
- 引擎核心层被迫向下层（math）产生业务耦合
- `_game/` 的 Runtime 反而没有自己的 System，变成空壳

### 1.4 `foundation` —— 过于单薄，未能承担"引擎标准库"职责

当前 `foundation/` 只有一个 `foundation_types.h`（类型别名 + 平台宏 + 断言）。

**问题**：UE 的 `base/` 是引擎的"STL 替代层"，包含类型、字符串、容器、数学基础、日志、IO 抽象。当前 `foundation` 太薄，导致本应属于基础层的工具无处可放，只能向上"溢出"到 `memory` 和 `core`。

---

## 二、参考架构：UE/chaos 与 Bevy 的分包哲学

### 2.1 UE/chaos 的分层（四阶段金字塔）

```
┌─────────────────────────────────────┐
│  第四阶段：client / editor / server │  ← 条件编译，可选
│  （渲染、动画、输入、相机、工具链）    │
├─────────────────────────────────────┤
│  第三阶段：core / common            │  ← 无条件编译，引擎运行时
│  （ECS、反射、资源、场景图、状态机）   │
├─────────────────────────────────────┤
│  第二阶段：base / mempool / platform│  ← 无条件编译，引擎标准库
│  （类型、容器、字符串、分配器、线程、窗口）│
├─────────────────────────────────────┤
│  第一阶段：构建系统                  │  ← 预编译、代码生成
└─────────────────────────────────────┘
```

**关键设计**：
- `base/` 是**零依赖**的"引擎 STL"，包含：类型别名、字符串、容器、基础数学、日志接口、路径抽象
- `mempool/` 只负责**分配器策略**（ObjectPool、Arena、Buddy、PageAllocator），依赖 `base/` 的类型
- `platform/` 只负责**OS 抽象**（线程、窗口、网络、硬件），依赖 `base/`
- `core/` 只负责**运行时框架**（ECS、反射、资源管理、场景图），依赖 `base/` + `mempool/` + `platform/`
- `common/` 是**通用中间件**（状态机、行为树、配置解析、调试绘制），依赖 `core/`

**循环依赖打破**：`base/` 的容器（如 `Vector<T, Allocator>`）通过模板参数接收 `mempool/` 的 allocator，但 `base/` 本身不依赖 `mempool/`。

### 2.2 Bevy 的 Crate 拆分（60+ 个独立 crate）

Bevy 作为 Rust 引擎，用 Cargo Workspace 拆成了极细的 crate：

| 层级 | 代表 Crate | 职责 |
|------|-----------|------|
| **核心层** | `bevy_ecs` | ECS 核心（World、Query、Schedule） |
| | `bevy_app` | App 生命周期、Plugin 系统 |
| | `bevy_reflect` | 反射系统（独立 crate！） |
| | `bevy_tasks` | 任务池与并行计算 |
| **基础工具** | `bevy_utils` | 工具类型与便利宏 |
| | `bevy_log` | 日志（独立 crate） |
| | `bevy_time` | Time 与 Timer |
| **功能层** | `bevy_render` | 渲染核心与 RenderGraph |
| | `bevy_asset` | 资源管理（独立 crate） |
| | `bevy_window` / `bevy_winit` | 窗口抽象与后端适配 |
| | `bevy_input` | 输入事件与 Action Mapping |
| | `bevy_pbr` / `bevy_sprite` / `bevy_ui` | 具体渲染功能 |
| | `bevy_transform` / `bevy_hierarchy` | 变换与层级（独立 crate！） |

**关键设计**：
- **细粒度**：甚至连 `Transform` 和 `Hierarchy` 都是独立 crate
- **同名一致性**：feature 名称 = crate 名称 = 模块名称（`bevy_ui` → `bevy::ui`）
- **依赖链自动推导**：开 `bevy_pbr` 自动拉取 `bevy_light` + `bevy_material` + `bevy_core_pipeline`
- **no_std 优先**：`bevy_ecs` 可在无标准库环境运行，上层渲染按需叠加

### 2.3 两种模式的对比与我们的取舍

| 维度 | UE/chaos | Bevy | 我们的建议 |
|------|----------|------|-----------|
| **拆分粒度** | 粗（~10 个大模块） | 极细（60+ crate） | **中粗**（8~15 个模块） |
| **容器归属** | `base/` 统一 | `bevy_utils` + 分散 | **`base/` 统一** |
| **字符串归属** | `base/` | `bevy_utils` | **`base/` 统一** |
| **ECS 边界** | `core/` 含 ECS+反射+资源 | `bevy_ecs` 纯 ECS | **`core/` 纯 ECS**，反射/资源未来可拆 |
| **Transform** | `core/` 场景图 | `bevy_transform` 独立 | **短期在 `core/`，中期可独立** |
| **平台抽象** | `platform/` 独立 | `bevy_winit` 后端适配 | **`window/` 逐步扩展为 `platform/`** |

> **取舍原则**：个人/小团队引擎不应照搬 Bevy 的 60+ crate（维护成本过高），也不应像 UE 那样把太多东西塞进 `core/`。取两者之长：**模块数控制在 10~15 个，每个模块语义单一、边界清晰。**

---

## 三、近期改进方案（可立即执行）

### 3.1 新建/扩展 `base/` —— 引擎标准库

**动作**：将 `foundation/` 扩展为 `base/`，成为引擎的"STL 替代层"。

```
_engine/source/base/
├── foundation_types.h      ← 原 foundation 内容
├── string_id.h             ← 从 core/ 迁移
├── small_string.h          ← 从 core/ 迁移
├── string_format.h         ← 从 core/ 迁移
├── string_intern_pool.h/.cpp ← 从 core/ 迁移
├── dynamic_array.h         ← 从 memory/ 迁移
├── hash_map.h              ← 从 memory/ 迁移
├── inline_array.h          ← 从 memory/ 迁移
├── ring_buffer.h           ← 从 memory/ 迁移
└── CMakeLists.txt
```

**边界约定**：
- `base/` **零依赖**：不 `#include` 引擎任何其他模块
- `base/` 的容器通过模板参数 `AllocatorT` 支持自定义分配器，但默认使用裸 `new/delete`
- `memory/` 的分配器（如 `FrameArena`）可以被传入 `base/` 的容器，但 `base/` 不依赖 `memory/`

### 3.2 收缩 `memory/` —— 纯分配器模块

**动作**：迁出所有容器，只保留分配器家族。

```
_engine/source/memory/
├── iallocator.h            ← 统一虚接口
├── allocator.h             ← 全局分配器
├── debug_allocator_wrapper.h/.cpp
├── frame_arena.h/.cpp
├── object_pool.h
└── CMakeLists.txt
```

**语义澄清**：`memory/` 回答的问题是"**如何分配内存**"，不是"**用什么数据结构**"。

### 3.3 收缩 `core/` —— 纯 ECS 运行时框架

**动作**：迁出字符串系统，只保留 ECS 核心。

```
_engine/source/core/
├── world.h/.cpp
├── component_array.h
├── sparse_set.h/.cpp
├── archetype_chunk.h
├── command_buffer.h
├── query.h
├── type_registry.h/.cpp
├── types.h
└── CMakeLists.txt
```

**边界约定**：
- `core/` 回答"**游戏世界是什么**"（实体、组件、查询、调度）
- 不回答"**字符串怎么存**"（去 `base/`）
- 不回答"**物体怎么动**"（去 gameplay）

### 3.4 拆分 `system/` —— 调度器 vs Gameplay 系统

**动作**：把具体 Gameplay System 下放到 `_game/`，`system/` 只保留调度基础设施。

```
_engine/source/system/
├── scheduler.h             ← System 调度、Tick 阶段、依赖图
├── system_init.cpp         ← 注册引擎级 System（如未来的渲染系统）
└── CMakeLists.txt

_game/source/runtime/
├── movement_system.h/.cpp          ← 从 system/ 迁移
├── transform_propagation_system.h/.cpp ← 从 system/ 迁移
└── ...（未来新增 Gameplay System）
```

**边界约定**：
- `system/`（引擎）回答"**System 怎么被调度**"
- `_game/runtime/`（游戏）回答"**具体 System 做什么逻辑**"

### 3.5 迁移后的模块依赖图

```
base/（零依赖）
    │
    ├──► memory/      ← base/ 的类型和断言
    │
    ├──► math/        ← base/ 的类型
    │
    ├──► log/         ← base/ 的类型和宏
    │
    └──► core/        ← base/ 的容器 + memory/ 的分配器
            │
            ├──► system/    ← core/ 的 World + base/ 的类型
            │
            └──► _game/runtime/  ← system/ 的调度 + core/ 的 ECS + math/ 的 Transform

window/（依赖 base/，未来扩展为 platform/）
    │
    └──► render/      ← window/ 的上下文 + core/ 的 ECS 组件

imgui/（依赖 window/ + render/ + log/）

bridge/（依赖 core/ + system/）
```

---

## 四、中期规划（Phase 4~5 阶段）

### 4.1 将 `window/` 扩展为 `platform/`

当前 `window/` 已包含窗口和输入事件。未来如果增加：
- 线程池与任务系统
- 文件 IO 与 VFS 底层
- 网络 Socket 抽象
- CPU/SIMD 硬件检测
- 动态库加载

建议将 `window/` 重命名为 `platform/`，内部按子系统组织：

```
_engine/source/platform/
├── window/
│   ├── window.h
│   ├── glfw_window.cpp
│   └── ...
├── input/
│   ├── input_event.h
│   ├── input_queue.cpp
│   └── ...
├── thread/           ← 未来新增
├── filesystem/       ← 未来新增
└── network/          ← 未来新增（单机预留接口）
```

### 4.2 考虑从 `core/` 拆分出 `reflect/` 和 `asset/`

参考 Bevy 的 `bevy_reflect` 和 `bevy_asset` 是独立 crate：

| 模块 | 职责 | 拆分时机 |
|------|------|---------|
| `reflect/` | 类型注册、属性遍历、序列化、Inspector 自动生成 | 当反射系统足够复杂（>5 个核心文件）时 |
| `asset/` | 异步加载、Handle、引用计数、热重载、Pak | 当资源管理实现时（阶段 5） |
| `event/` | 事件总线、Pub-Sub、延迟队列 | 当事件系统独立化时 |

> **判断标准**：一个子系统如果同时被 `core/` 内部和外部模块（如 `render/`、`_game/`）依赖，且文件数 >5，就值得独立。

### 4.3 引入 `common/` —— 通用中间件层

参考 chaos 的 `common/` 模块，在 `core/` 和具体业务之间建立一个"比 core 更业务、比 gameplay 更通用"的层：

```
_engine/source/common/          ← 未来新增
├── scene_graph.h               ← 场景图（Transform 层级、脏标记传播）
├── prefab.h                    ← Prefab 资产结构
├── serialization.h             ← 序列化/反序列化框架
├── state_machine.h             ← 状态机基础
├── debug_draw.h                ← 调试绘制接口
└── ...
```

> **为什么现在不加？** 当前阶段（Phase 0~1）还没有这些系统的实现。等阶段 4 的核心运行时闭环完成后，自然会有拆分需求。

### 4.4 `render/` 的的未来内部分层

当前 `render/` 只有一个 OpenGL 后端。未来实现 RHI 后，内部可按以下方式组织：

```
_engine/source/render/
├── rhi/                        ← RHI 抽象层（IRHIBuffer、IRHITexture、IRHIPipelineState）
│   ├── render_backend.h
│   ├── opengl_backend.cpp
│   └── vulkan_backend.cpp      ← 未来新增
├── renderer/
│   ├── render_graph.h          ← RenderGraph
│   ├── render_pass.h
│   └── ...
├── material/
│   ├── material.h
│   └── shader_cache.h
├── 2d/                         ← 2D 渲染、Sprite、UI 渲染
└── post_process/               ← 后处理栈
```

> **注意**：`render/` 内部子目录的拆分属于"模块内部分层"，不需要在 Source Forest 中创建独立的 CMake 项目。只有当某个子系统需要被 `render/` 外部独立依赖时（如 `2d/` 被 `ui/` 依赖但不需要 `3d/`），才考虑拆分为独立模块。

---

## 五、执行顺序与风险规避

### 5.1 推荐的迁移顺序

```
Step 1: 创建 base/，将 foundation/ 内容移入
        └─ 风险最低，只是文件搬家，改 #include 路径

Step 2: 将 core/ 的字符串系统迁移到 base/
        └─ 需同步修改所有引用 `string_id.h` 等文件的模块

Step 3: 将 memory/ 的容器迁移到 base/
        └─ 需同步修改 core/、math/ 等模块的 #include 和 CMakeLists.txt

Step 4: 验证全量编译通过

Step 5: 将 system/ 的 Gameplay System 迁移到 _game/runtime/
        └─ 需调整 _game/ 的 cmake_projects.json 和依赖声明

Step 6: 更新所有 AGENTS.md 文档
```

### 5.2 风险点

| 风险 | 应对措施 |
|------|---------|
| 大量 `#include` 路径变更导致编译失败 | 使用 IDE 全局替换 + 分步验证 |
| `CMakeLists.txt` 的链接库声明遗漏 | 每次迁移后必须全量编译 |
| 循环依赖（如 base/ 需要 memory/ 的 allocator） | 用模板参数解耦：`Vector<T, AllocatorT = DefaultHeapAllocator>` |
| 第三方代码（如 ImGui）依赖路径变化 | ImGui 不依赖引擎内部模块，不受影响 |

---

## 六、总结：模块语义速查表

迁移完成后，每个模块的"一句话职责"应该是：

| 模块 | 一句话职责 | 类比 |
|------|-----------|------|
| `base/` | 引擎标准库：类型、字符串、容器、基础工具 | UE `base/`、C++ STL |
| `memory/` | 内存分配策略：Arena、Pool、调试工具 | UE `mempool/` |
| `math/` | 数学运算：向量、矩阵、四元数、几何体 | UE `base/math` |
| `log/` | 分级异步日志：多 Sink、过滤、结构化输出 | UE `base/log`、Bevy `bevy_log` |
| `platform/`（现 `window/`） | OS 抽象：窗口、输入、线程、文件、网络 | UE `platform/`、Bevy `bevy_winit` |
| `core/` | ECS 运行时框架：World、Query、Archetype、TypeRegistry | Bevy `bevy_ecs` |
| `system/` | System 调度基础设施：Scheduler、Tick 阶段 | Bevy `bevy_app/schedule` |
| `render/` | 图形渲染：RHI、RenderGraph、材质、后处理 | UE `client/renderer`、Bevy `bevy_render` |
| `imgui/` | 调试 UI：ImGui 面板、引擎可视化工具 | — |
| `bridge/` | AI 桥接：MCP、Agent 协议、工具注册 | — |
| `_game/runtime/` | 游戏玩法系统：Movement、Transform、具体逻辑 | UE `wolfgang/` |

---

> **核心原则**：模块名称 = 模块语义。如果打开一个目录后发现"这东西好像不该在这里"，那它大概率就不该在这里。
