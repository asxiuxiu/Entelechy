# Editor 模块
> 路径：`_engine/source/editor`

## 一句话职责

ECS 运行时与 ImGui 表现层之间的调试/编辑器桥接层：ECS Inspector 面板、Atom 类型绘制注册。

## 关键文件
| 文件 | 职责 |
|------|------|
| `editor_init.cpp` | 初始化桩：注册内置 Atom 类型的 ImGui 绘制回调 |
| `editor_init.h` | `initEditor()` 声明 |
| `editor_panels.cpp` | ECS Inspector 面板：Entity 列表 + Component 详情（基于 `AtomRegistry` 零硬编码绘制） |
| `editor_atom_registry.cpp` | ImGui 后端：`AtomRegistry::registerBuiltinAtoms()` 实现，注册原子类型的 Inspector 绘制函数 |
| `editor_panels.h` | `buildECSInspector()` 声明 |

## 重要入口
- 改 **ECS Inspector 的实体列表、组件字段或交互行为** → 动 `editor_panels.cpp`
- 改 **Inspector 原子类型绘制注册** → 动 `editor_atom_registry.cpp`

## 依赖关系
- 向上依赖：
  - [ImGui 模块](../imgui/AGENTS.md)（ImGui 绘制 API、DockSpace 布局）
  - [ECS 模块](../ecs/AGENTS.md)（World、Scheduler、AtomRegistry、TypeRegistry、Entity、组件类型）
  - [Core 模块](../core/AGENTS.md)（StringInternPool、数学类型）
- 被依赖：
  - Runtime（主循环调用 `buildECSInspector`）

## 架构决策
- **Inspector 零硬编码**：`drawField` 优先查询 `AtomRegistry`，其次硬编码展开 `Mat4`，再次递归 `TypeDesc`/`ComponentDesc`；新增组件类型无需修改 Editor 代码即可显示
- **Atom 绘制注册归属**：`AtomRegistry::registerBuiltinAtoms()` 的 ImGui 后端实现位于本模块而非 EcsLib，使 EcsLib 保持 headless 可用
- **初始化顺序**：`initEditor()` 在 `initEcs()` 和 `initImGui()` 之后调用，确保 AtomRegistry 和 ImGui 上下文就绪

## 技术债务

> 统一维护于 [TODO.md](../../../../TODO.md)。
