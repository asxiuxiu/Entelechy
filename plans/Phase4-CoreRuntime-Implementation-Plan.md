# Phase 4：核心运行时闭环 — 修订实施计划（未完成项）

> **状态**：本计划为修订版。原 Phase 4 计划中的批次 A（数据层）、批次 C（调度 + 事件）及批次 0 的 ThreadPool 已落地，已从本文档中删除。本文仅追踪**实际尚未完成**的能力缺口。
> **目标**：补齐 Entelechy 从"可运转的 ECS demo"升级为"能描述、保存、加载并模块化扩展一个世界"的最后几块拼图。

---

## 一、现状诊断：剩余缺口

| 子模块 | 已有实现 | 缺失（本计划要补齐） | 优先级 |
|--------|---------|---------------------|--------|
| **3.5 文件 IO / VFS** | `IMountPoint` + `FileSystemMountPoint` + `MemoryMountPoint` 已存在；基础 `VFS` 类已存在 | `Path` 类（统一 `/` 分隔符）；VFS 挂载命名（逻辑路径 → 后端映射）；后挂载优先覆盖语义 | 高 |
| **3.2 场景图与变换** | `Transform` / `GlobalTransform` / `TransformTreeChanged` 已存在；`hierarchy.h` 辅助函数已存在；`TransformPropagationSystem` 已存在 | **rank 分桶并行**：当前实现是每帧全量 `std::sort` + 串行遍历，需改为按 rank 分桶后同桶内 `parallel_for`；脏标记向上传播剪枝 | 高 |
| **3.4 反射系统** | `AtomRegistry` 已独立为 `core/atom_registry.h`；`inspector_reflection.h` 接口已预留；`TypeRegistry` 管理组件 ID/掩码/字段描述 | **分层类型描述**：`TypeDesc` 需支持 `TypeKind::Atom` / `Composite` / `Array`；`FieldDesc` 需从字符串类型名升级为 `atom_name` / `subtype` union；`inspector_reflection.cpp` 需实现递归绘制；`imgui_panels.cpp` 需从硬编码/半自动改为完全基于反射 | 高 |
| **3.7 Prefab 与数据层** | 无 | `Prefab` 结构 + `PrefabInstance` 溯源组件；`PrefabOverride` 全局追踪表；`SceneSerializer` 场景 JSON 序列化/反序列化；嵌套 Prefab | 中 |
| **3.8 Plugin 与模块系统** | 无 | `IPlugin` 接口；`App` 容器（World + Scheduler + PluginRegistry）；`PluginRegistry` 运行时加载顺序与依赖检查；代码生成器 `gen_reflection.py` | 中 |

---

## 二、总体设计哲学：一步到位，无过渡

1. **不重复造过渡轮**：当前已消除所有 STL 依赖（`std::vector` → `DynamicArray`、`std::string` → `SmallString`、`std::unordered_map` → `HashMap`），剩余工作直接在最终架构上补全。
2. **性能与复杂度权衡取笔记共识**：
   - 场景图脏标记：**子树级 dirty flag 向上传播**（Bevy 0.16 验证，127k 场景 1.1ms → 0.1ms）。
   - 反射：**分层类型描述 + 原子类型注册表**，直接支持 `Vec3` / `Quat` / `Mat4` 递归展开。
   - Prefab：**实例化时合并 + `PrefabInstance` 溯源**（分支 C），运行时零开销，编辑器可追踪。
   - Plugin：**静态库 + 运行时逻辑组合**（Bevy 风格），避开 C++ 动态库的符号地狱；显式依赖用 CMake `PUBLIC/PRIVATE` 强制执行。
3. **AI 可观测性内建**：所有新增系统默认暴露 Schema 与查询接口，为阶段 8 的 MCP / AgentBridge 预留零成本对接路径。

---

## 三、子模块详细规划（仅剩余项）

---

### 3.5 文件 IO 与虚拟文件系统（完善）

**决策摘要**：
- **Path 抽象**：统一用 `/` 作为内部分隔符存储，仅在调用 OS API 时按平台转换。支持拼接 `operator/`、扩展名提取 `extension()`、文件名提取 `fileName()`、绝对路径判断 `isAbsolute()`。
- **VFS 挂载命名**：当前 `VFS::mount(IMountPoint*)` 缺少逻辑路径名。改为 `mount(StringId logicalPath, IMountPoint* backend)`，按挂载顺序查找时**后挂载优先**（天然支持 MOD/DLC 覆盖）。
- **跨平台文件后端**：`FileSystemMountPoint` 内部封装 `fopen` / `CreateFileW` / `open` 等平台差异。Windows 用 `CreateFileW` 支持宽字符/长路径。
- **Pak 预留**：`PakMountPoint` 接口预留（阶段 9 实现），本阶段不实现。

**文件变更**：

| 文件 | 动作 | 内容 |
|------|------|------|
| `base/path.h` | **新建** | `Path` 类：内部存储 `SmallString`，统一 `/` 分隔；`operator/`、`extension()`、`fileName()`、`stem()`、`isAbsolute()`。 |
| `io/vfs.h/.cpp` | **修改** | `mount(const char* logicalPath, IMountPoint* backend)` 增加逻辑路径名；`readFile(Path)` / `exists(Path)` 按挂载顺序从后向前查找（后挂载优先）；`unmount(const char* logicalPath)` 按名卸载。 |

**关键接口**：
```cpp
// vfs.h
class VFS {
public:
    void mount(const char* logicalPath, IMountPoint* backend);
    void unmount(const char* logicalPath);
    DynamicArray<u8> readFile(const Path& path) const;
    bool writeFile(const Path& path, Slice<u8> data) const;
    bool exists(const Path& path) const;
private:
    struct MountEntry { StringId name; IMountPoint* backend; };
    DynamicArray<MountEntry> m_mounts; // 后挂载在前，查找时优先匹配
};
```

**验收**：
- `Path("assets") / "textures" / "hero.png"` 跨平台正确拼接；
- `VFS::mount("/assets", new FileSystemMountPoint("./assets"))` 后，`vfs.readFile("/assets/config.json")` 能读到内容；
- 后挂载的同名路径优先覆盖先挂载的。

---

### 3.2 场景图与变换（并行化 + 脏标记剪枝）

**决策摘要**：
- **层级存储**：双向 Relationship —— `ChildOf`（子节点存父）+ `Children`（父节点存子列表）。与 `Transform` / `GlobalTransform` 解耦。`hierarchy.h` 辅助函数已存在，保留。
- **变换存储**：`Transform`（本地 TRS + dirty bit）+ `GlobalTransform`（世界 Mat4 缓存）已有，保留。
- **脏标记**：`TransformTreeChanged` ZST（Zero-Sized Type 标记结构体），**向上传播**到根。更新阶段从根向下遍历，遇到无标记子树直接剪枝。
- **更新顺序**：`rank` 分桶。挂接/解挂时 BFS 更新受影响子树的 `rank`。`TransformPropagationSystem` 按 `rank` 从小到大分批执行，同 rank 内通过线程池 `parallel_for` 并行。
- **动态挂接**：暴露两种语义 —— `set_parent(child, parent)`（默认 KeepRelative）+ `set_parent_in_place(child, parent)`（KeepWorld，编辑器刚需）。
- **消费者解耦**：Transform 传播放在 `PostUpdate` Phase，渲染/物理在之后通过 `Query<&GlobalTransform>` 读取。

**文件变更**：

| 文件 | 动作 | 内容 |
|------|------|------|
| `math/transform_component.h` | 保留 | `Transform` / `GlobalTransform` / `TransformTreeChanged` 已存在，无需改动。 |
| `math/transform_propagation_system.cpp/.h` | **重写** | 按 rank 分桶实现：① `markDirtyTrees`（向上传播 `TransformTreeChanged`）；② `propagateByRank`（按 rank 桶顺序，同 rank 用 `ThreadPool::parallelFor` 并行计算 `GlobalTransform = parent_world * local`）。支持脏标记剪枝。 |
| `core/world.h` | 修改 | 新增 `setParentInPlace(Entity child, Entity parent)` 模板 API，内部通过 `CommandBuffer` 延迟执行，保持 KeepWorld（子节点世界坐标不变，只改本地坐标）。 |

**关键算法状态图（向上传播）**：
```
变更前：            变更后（E.local_position += (1,0,0)：
        A                   A[*]
      /   \               /    \
     B     C             B[*]   C
    / \                  /  \
   D   E                D    E[*]

更新遍历：
- A 有 [*] → 更新 A，继续向下
- B 有 [*] → 更新 B，继续向下
- D 无 [*] → 【剪枝，跳过 D 整棵子树】
- E 有 [*] → 更新 E
- C 无 [*] → 【剪枝，跳过 C 整棵子树】
```

**验收**：
- 窗口里有 3~5 个彩色立方体在旋转/平移；
- Inspector 中修改 `Transform.position`，立方体立刻移动；
- 立方体之间有父子层级，修改父节点时子节点跟随；
- 相比当前串行实现，在 1000+ 实体层级场景下有可观测的性能提升（或至少不劣化）。

---

### 3.4 反射系统（大幅扩展）

**决策摘要**：
- **分层类型描述（分支 B）**：`TypeDesc` 支持 `TypeKind::Atom` / `TypeKind::Composite` / `TypeKind::Array`。复合类型通过 `subtype` 指针递归展开。
- **原子类型注册表（AtomRegistry）**：`core/atom_registry.h` 已存在，每个原子类型注册一组函数指针（`inspectorDraw` / `serializeJson` / `deserializeJson` / `copy` / `equal`）。`FieldDesc` 用 `StringId atom_name` 指向原子类型，或用 `const TypeDesc* subtype` 指向复合类型。
- **与 ECS 对接**：System 热路径**绝不走反射**；反射仅服务 Inspector、序列化、AI Schema。ECS 与反射共用 `StringId` 标识，保持标识一致，不做强制双向绑定。
- **AI Schema**：运行时暴露 `listComponentTypes()` / `getComponentSchema(StringId name)` JSON 端点。
- **代码生成器**：提供 `scripts/tools/gen_reflection.py`（Python + 正则，约 50 行核心逻辑），扫描 `REFLECT()` / `REFLECT_FIELD()` 标记，生成注册代码。本阶段先手动宏注册，同时把生成器脚本和 CMake 预构建步骤搭好，组件超过 20 个时可一键迁移。

**文件变更**：

| 文件 | 动作 | 内容 |
|------|------|------|
| `core/type_registry.h/.cpp` | **重写** | `TypeDesc` 增加 `TypeKind kind` + `DynamicArray<FieldDesc> fields`。`FieldDesc` 增加 `StringId atom_name` / `const TypeDesc* subtype` union（通过 `kind` 区分）。提供 `findField(StringId name)` / `getSchemaJson()`。 |
| `core/atom_registry.h/.cpp` | **补全 .cpp** | 当前只有 `.h`，需实现 `instance()` / `registerAtom()` / `find()` / `tryDraw()` / `registerBuiltinAtoms()`（注册 `f32`, `i32`, `bool`, `StringId`, `Vec3`, `Quat`, `Mat4`, `Color`, `Entity` 等）。 |
| `core/inspector_reflection.h/.cpp` | **新建 .cpp** | 实现 `inspectorDrawComponent()`：递归遍历 `FieldDesc`，Atom 类型调用 `AtomRegistry::draw`，Composite 类型递归 TreeNode。为 ImGui 面板提供零硬编码绘制路径。 |
| `scripts/tools/gen_reflection.py` | **新建** | 最小可行代码生成器：扫描 `_engine/source/**/*.h` 中的 `REFLECT()` 结构体，生成 `generated/reflection.gen.cpp`。CMake `add_custom_command` 集成。 |
| `imgui/imgui_panels.cpp` | **重写** | ECS Inspector 面板从硬编码/半自动改为**完全基于反射**：遍历 `TypeRegistry` 所有组件类型 → 对每个实体查询组件 → 调用 `inspectorDrawComponent()`。 |

**关键数据结构**：
```cpp
enum class TypeKind { Atom, Composite, Array };

struct FieldDesc {
    StringId name;
    usize offset;
    usize size;
    TypeKind kind;
    union {
        StringId atom_name;      // kind == Atom
        const TypeDesc* subtype; // kind == Composite
    };
    // 本阶段先不做注解，预留 DynamicArray<Annotation> annotations;
};

struct TypeDesc {
    StringId name;
    usize size;
    usize alignment;
    TypeKind kind; // 顶层通常为 Composite
    DynamicArray<FieldDesc> fields;
};
```

**验收**：
- Inspector 能自动展开 `Vec3` / `Quat` / `Mat4` 的字段；
- 新增一个自定义组件（如 `struct MyComp { f32 speed; Vec3 offset; }`），无需修改 ImGui 代码，Inspector 自动能绘制；
- `AtomRegistry::registerBuiltinAtoms()` 覆盖所有引擎内置原子类型；
- `gen_reflection.py` 可运行并生成通过编译的 `.cpp`。

---

### 3.7 Prefab 与数据层（从零新建）

**决策摘要**：
- **Prefab 结构**：`Prefab` 资源包含 `defaults`（组件类型 → JSON 字段默认值）+ `children`（嵌套 Prefab 引用列表）。不存储运行时状态。
- **实例化策略（分支 C）**：`instantiate_prefab()` 一次性深拷贝默认值到实体组件，再应用覆盖值。实例化后实体为普通 ECS 实体，System 迭代零开销。
- **溯源标记**：`PrefabInstance { AssetID prefab_id; u32 instance_index; }` 组件附加到根实体，编辑器/热重载时读取。
- **覆盖追踪（分支 B）**：独立的 `PrefabOverride` 全局表（不嵌入组件内部），记录 `Entity + ComponentType + FieldPath → JSON value`。Inspector 中通过查询该表判断字段是否为覆盖值。
- **场景序列化**：基于反射系统自动将 `World` 状态导出为 JSON。遍历所有存活实体 → 遍历其实体组件 → 通过 `TypeDesc` + `AtomRegistry` 递归序列化字段。反序列化时逆过程重建实体和组件。
- **数据驱动生成**：支持从 JSON 场景文件直接加载整个 `World`，无需手写 C++ 创建代码。

**文件变更**：

| 文件 | 动作 | 内容 |
|------|------|------|
| `core/prefab.h/.cpp` | **新建** | `struct Prefab { AssetID id; HashMap<StringId, JsonValue> defaults; DynamicArray<PrefabChild> children; };`。提供 `instantiate(World&, const Prefab&, const JsonValue* overrideJson)`。 |
| `core/prefab_instance.h` | **新建** | `struct PrefabInstance { AssetID prefabID; u32 instanceIndex; };`。 |
| `core/prefab_override.h/.cpp` | **新建** | `struct PrefabOverride { Entity target; StringId componentType; StringId fieldPath; JsonValue value; };`。`PrefabOverrideTable`：增删查改覆盖值；`revertToPrefab(Entity, componentType, fieldPath)`。 |
| `core/scene_serializer.h/.cpp` | **新建** | `SceneSerializer`：利用 `TypeRegistry` + `AtomRegistry` 实现 `save(World&, VFSPath)` 和 `load(World&, VFSPath)`。JSON 格式，人类可读。 |
| `core/world.h` | 修改 | 增加 `spawnFromPrefab(const Prefab&, const JsonValue* override = nullptr)` API，内部走 `CommandBuffer` 批量提交（保证原子性）。 |

**关键接口**：
```cpp
// prefab.h
Entity instantiatePrefab(World& world, const Prefab& prefab,
                         const JsonValue* overrides = nullptr,
                         Entity parent = Entity::invalid);

// scene_serializer.h
class SceneSerializer {
public:
    bool save(const World& world, const Path& path);
    bool load(World& world, const Path& path); // 会清空现有 world 或增量加载
};
```

**验收**：
- 能保存当前场景为 JSON，重启后加载恢复（立方体位置、层级、颜色）；
- 从 Prefab 实例化 10 个相同实体，修改其中一个的字段后 `revertToPrefab()` 能恢复默认值；
- 嵌套 Prefab（如"带炮塔的坦克"）能正确实例化并保持层级关系。

---

### 3.8 Plugin 与模块系统（运行时部分新建）

**决策摘要**：
- **物理边界**：保持现有 CMake 静态库划分（Base / Core / Math / Memory / Window / Render / ImGui / Log / Motor / Bridge / Runtime），不贸然引入动态库。
- **运行时组合（Bevy 风格）**：引入 `IPlugin` 接口 + `App` 容器。`App` 聚合 `World` + `Scheduler` + `PluginRegistry`。每个 Plugin 的 `build(App&)` 方法中注册自己的 System、组件、反射类型。
- **显式依赖**：CMake 层面继续用 `target_link_libraries(... PUBLIC/PRIVATE)` 强制执行编译期依赖。`PluginRegistry` 在运行时做加载顺序检查（若 A Plugin 声明依赖 B，但 B 未加载则 `LOG_FATAL`）。
- **生命周期**：`IPlugin { virtual void build(App&) = 0; virtual void setup(App&) {} /* 所有 Plugin build 完后调用 */ virtual void teardown(App&) {} };`。
- **热插拔预留**：`IPlugin` 预留 `teardown()`，但本阶段只做逻辑级卸载（注销 System），不做 DLL 卸载。

**文件变更**：

| 文件 | 动作 | 内容 |
|------|------|------|
| `core/plugin.h` | **新建** | `struct IPlugin { virtual ~IPlugin() = default; virtual void build(App&) = 0; virtual void setup(App&) {} virtual void teardown(App&) {} };`。 |
| `core/app.h/.cpp` | **新建** | `class App { World world; Scheduler scheduler; PluginRegistry plugins; void addPlugin(IPlugin*); void run(); /* 主循环：scheduler.tick(world, dt) + render */ };`。 |
| `core/plugin_registry.h/.cpp` | **新建** | `PluginRegistry`：管理 `IPlugin*` 列表，按依赖拓扑排序调用 `build()` → `setup()`。提供 `findPlugin(StringId name)`。 |
| `_game/source/runtime/game_runtime.cpp` | **重写** | 不再裸初始化各模块，而是构造 `App`，通过 `app.addPlugin()` 注册 `WindowPlugin`、`RenderPlugin`、`ImGuiPlugin`、`LogPlugin`、`CorePlugin`（注册 ECS 内置组件和 System）、`MotorPlugin`、`BridgePlugin`、`GamePlugin`（游戏层逻辑）。`main.cpp` 最终简化为 `App app; app.addPlugin(...); app.run();`。 |

**关键接口**：
```cpp
// app.h
class App {
public:
    World& world() { return m_world; }
    Scheduler& scheduler() { return m_scheduler; }
    void addPlugin(IPlugin* plugin);
    void run(); // 接管主循环

private:
    World m_world;
    Scheduler m_scheduler;
    PluginRegistry m_plugins;
    bool m_running = true;
};

// plugin.h 示例用法
struct MotorPlugin : IPlugin {
    void build(App& app) override {
        app.scheduler().registerSystem({
            .name = SID("MovementSystem"),
            .system = &m_movement,
            .phase = static_cast<u8>(DefaultPhase::Update),
            .reads = { TypeRegistry::instance().getTypeID<Velocity>() },
            .writes = { TypeRegistry::instance().getTypeID<Position>() },
        });
    }
    MovementSystem m_movement;
};
```

**验收**：
- 新增一个 `RotationPlugin`（注册一个让立方体旋转的 System），仅需在 `game_runtime.cpp` 中 `app.addPlugin(&rotationPlugin)`，无需改核心代码；
- `App::run()` 接管主循环，窗口正常显示、System 正常 tick；
- Plugin 的 `setup()` 在所有 `build()` 之后被正确调用。

---

## 四、实施路线图（4 个批次）

> **原则**：每完成一个批次，必须能通过 Debug 构建运行，且验收标准中的可见项逐步点亮。

### 批次 0：VFS 完善（Path + 挂载语义）
- **目标**：补齐 VFS 的 Path 抽象和挂载命名/覆盖语义，为场景序列化、Prefab 加载提供文件 IO 硬依赖。
- **文件**：`base/path.h`；`io/vfs.h/.cpp`（修改）
- **验收**：
  - `Path("assets") / "textures" / "hero.png"` 跨平台正确拼接；
  - `VFS::mount("/assets", new FileSystemMountPoint("./assets"))` 后，`vfs.readFile("/assets/config.json")` 能读到内容；
  - 后挂载的同名路径优先覆盖先挂载的。

### 批次 1：Transform 传播并行化
- **目标**：将当前串行的 `TransformPropagationSystem` 升级为 rank 分桶 + `parallel_for` 并行，引入脏标记剪枝。
- **文件**：`math/transform_propagation_system.cpp/.h`（重写）；`core/world.h`（新增 `setParentInPlace`）
- **验收**：
  - 窗口里有 3~5 个彩色立方体在旋转/平移；
  - Inspector 中修改 `Transform.position`，立方体立刻移动；
  - 父子层级关系正确；
  - 1000+ 实体场景下相比串行实现不劣化（或有提升）。

### 批次 2：反射系统扩展（TypeDesc 分层 + Inspector 零硬编码）
- **目标**：反射系统支持复合类型递归展开，Inspector 面板完全基于反射绘制，新增组件无需改 ImGui 代码。
- **文件**：`core/type_registry.h/.cpp`（重写）；`core/atom_registry.cpp`（补全）；`core/inspector_reflection.cpp`（新建）；`imgui/imgui_panels.cpp`（重写）；`scripts/tools/gen_reflection.py`（新建）
- **验收**：
  - Inspector 能自动展开 `Vec3` / `Quat` / `Mat4`；
  - 新增自定义组件后 Inspector 自动绘制；
  - `gen_reflection.py` 可运行并生成通过编译的 `.cpp`。

### 批次 3：Prefab + Plugin（数据持久化 + 模块化）
- **目标**：场景可保存加载，功能可通过 Plugin 扩展。
- **文件**：`core/prefab.h/.cpp`、`core/prefab_instance.h`、`core/prefab_override.h/.cpp`、`core/scene_serializer.h/.cpp`、`core/plugin.h`、`core/app.h/.cpp`、`core/plugin_registry.h/.cpp`、`_game/source/runtime/game_runtime.cpp`
- **验收**：
  - 能保存当前场景为 JSON，重启后加载恢复（立方体位置、层级、颜色）；
  - 新增 `RotationPlugin` 仅需 `app.addPlugin(&rotationPlugin)`；
  - `App::run()` 接管主循环，窗口正常显示；
  - Prefab 实例化 + 覆盖 + revert 正确工作。

---

## 五、风险与回退策略

| 风险 | 影响 | 缓解策略 |
|------|------|---------|
| `TypeDesc` 分层重写波及面广（`TypeRegistry`、`REFLECT_COMPONENT` 宏、Inspector） | 批次 2 延期 | 保持旧 `ComponentDesc` 兼容 API 一周，渐进替换，每改一个文件就构建验证。 |
| `TransformPropagationSystem` rank 分桶并行引入 bug | 批次 1 延期 | 先实现串行版 rank 分桶（不按并行），验证正确性后再启用 `parallel_for`。 |
| 反射递归绘制导致 ImGui 面板性能差 | 批次 2 体验差 | 对 100 个以内实体完全可接受。若实体数 > 500 时 Inspector 卡顿，再为高频组件（Transform）手写特化绘制路径。 |
| Prefab 序列化依赖 VFS | 批次 3 阻塞 | 批次 0 已明确实现 VFS，不再存在就绪风险。若 VFS 接口在批次 3 发现不够用，临时在 `SceneSerializer` 内部扩展 VFS 能力，不改动 VFS 公共接口。 |
| Plugin 系统重构导致 main.cpp / build 系统变动大 | 批次 3 构建失败 | 保持现有 `launch/generator.py` 和 `cmake_projects.json` 不变。`App` 只是运行时容器，不改动构建系统模块划分。 |

---

## 六、与后续阶段的衔接

| 本阶段产出 | 阶段 5（渲染管线）如何使用 | 阶段 8（AI 桥接）如何使用 |
|-----------|------------------------|------------------------|
| `Transform` + `GlobalTransform` + 脏标记 + 并行传播 | 渲染 System 在 `RenderExtract` Phase 直接 `Query<&GlobalTransform, &Mesh>` 提取世界矩阵 | AI 通过 Schema 知道 `Transform` 字段结构，可远程修改 `position` |
| `AtomRegistry` + `TypeDesc` | 材质参数、着色器 uniform 的反射绑定 | AI Schema 导出直接消费 `TypeDesc::getSchemaJson()` |
| `Prefab` + `SceneSerializer` | 场景加载时自动创建带 `Mesh`/`Material` 的实体 | AI 可通过 MCP 调用 `save_scene` / `load_scene` |
| `Plugin` + `App` | `RenderPlugin` 注册 RHI System、`MaterialSystem` | `McpPlugin` 注册 AgentBridge System，不改动核心代码 |
| `Path` + VFS | 资源加载统一走 VFS，支持 MOD/DLC 覆盖 | AI 可请求 VFS 中的配置文件内容 |

---

> **本计划增量**：补齐 Phase 4 剩余的全部能力缺口，使 Entelechy 从"可编译的 ECS demo"升级为"能描述一个世界、保存它、加载它、并通过 Plugin 模块化扩展"的核心运行时闭环。
