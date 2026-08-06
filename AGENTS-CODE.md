# 代码规范

## 基础库优先

项目已封装的基础库（`core/` 模块）必须优先使用。详细对照表与底线规则见 [Core 模块规范](_engine/source/core/AGENTS.md)。

**底线**：基础库已有对应实现时，不允许以"习惯""省事"为由绕回 STL 或裸分配。

## 字符串体系

**决策口诀**：先问"要不要存进组件/字典键？" → 要，用 `StringId`；再问"要不要修改/拼接？" → 要，用 `String`；否则默认 `StringView`。

三种角色的详细约束、构造规则与已 StringId 化模块清单见 [Core 模块规范](_engine/source/core/AGENTS.md)。

## 基础设计优先：拒绝适配层与兼容后门

**核心原则**：基础类型和核心接口的设计必须一步到位正确。不能因为"上层还有旧代码在用"就给底层留兼容重载、隐式转换或适配层。

### 禁止行为

1. **禁止为旧调用方保留废弃 API**
   - ❌ `findComponent(const String& name)` + `findComponent(StringId name)` 双轨并存
   - ✅ 直接删除 `const String&` 版本，所有调用方同步改为 `StringId`

2. **禁止在基础类型上开隐式构造后门**
   - ❌ `StringId` 保留 public `StringId(const char*)` 让运行时可以直接构造
   - ✅ 运行时构造强制私有化，只能通过 `StringInternPool::intern()` 工厂生产

3. **禁止用适配层包装设计缺陷**
   - ❌ "先写个 `StringToIdAdapter` 过渡一下，以后再说"
   - ✅ 直接改到底层类型定义，让编译错误驱动上层同步修复

### 实施方式

- **改底层接口时，直接改到底**。不要保留旧签名，不要加 `_deprecated` 后缀，不要加 `[[deprecated]]` 拖时间。
- **让编译失败成为迁移清单**。编译器报错比文档更可靠，每个报错就是一个必须修复的调用点。
- **宁可单次改动量大，不要堆积 hack 代码**。单次大规模机械重构的成本远低于长期维护适配层。
- **上层调用方跟着改是义务，不是可选**。基础设计正确后，上层代码必须适配，没有"暂时兼容"的选项。

> **判断标准**：如果某个改动让代码"看起来对旧代码更友好"，但增加了接口复杂度或破坏了类型约束，那就是错的。基础设计永远优先于调用方便利。

## 命名空间

- **引擎层**：`namespace Entelechy { ... }`（大驼峰，与引擎同名）
- **游戏层**：`namespace game { ... }`（当前占位；后续具体游戏项目可改用独立代号）

## 断言与检查宏

禁止混用 `assert()` 或手写 `if (!cond) abort()`，必须按语义选择：

| 宏 | 语义 | 正确用法 |
|---|---|---|
| `CHECK(cond)` | 内部不变量 / 前置条件；Release 完全消除 | 无副作用。例如 `CHECK(index < m_count)` |
| `VERIFY(cond)` | 必须执行的表达式 + Debug 断言；Release 执行但不检查 | `cond` 有副作用。例如 `VERIFY(ptr = allocate())` |
| `ENSURE(cond)` | 非致命错误；记录日志，继续运行 | 可恢复场景。例如 `ENSURE(file.isOpen())` |
| `ENSURE_MSG(cond, fmt, ...)` | 带自定义消息的非致命检查 | 需要额外上下文时 |
| `STATIC_ASSERT(cond, msg)` | 编译期断言 | 类型大小、模板约束。例如 `STATIC_ASSERT(sizeof(Vec4) == 16, "Vec4 must be 16 bytes")` |

**选择口诀**：无副作用 + 严重错误 → `CHECK`；有副作用 + 严重错误 → `VERIFY`；可恢复 → `ENSURE`；编译期 → `STATIC_ASSERT`。

## 命名风格

| 类型 | 风格 | 示例 |
|------|------|------|
| 函数（含成员函数、自由函数） | `camelCase` | `initCore()`, `registerSystem()` |
| 类/结构体 | `PascalCase` | `World`, `AgentBridge` |
| 枚举值 | `PascalCase` | `Debug`, `KeyPress` |
| 成员变量（无论 public/private） | `m_snake_case` | `m_free_list`, `m_next_id` |
| 局部变量 / 参数 | `camelCase`（无 `m_` 前缀） | `dt`, `entityCount` |
| 宏/常量 | `UPPER_SNAKE_CASE` | `ENTITY_MAX` |

**自检口诀**：`m_` 之后全小写加下划线；函数首单词全小写；类名每个单词首字母大写；枚举值每个单词首字母大写；宏全部大写。

## 文件命名

**源文件一律使用 snake_case（小写下划线）**。头文件名默认取自动头内定义的**主类型**的 snake_case 化，但**不强求一个头文件只放一个类型**——强相关的类型（如一组 ECS 组件、公共的 POD 数据结构）可以聚类在同一个文件里，文件名反映这个聚类而不是单个类型。

| 场景 | 文件名 | 说明 |
|------|--------|------|
| 单一主类型 | `scene_loader.h` ↔ `class SceneLoader` | 直接用主类型名的 snake_case |
| 聚类（多个强相关类型） | `render_components.h` ↔ RenderMesh / RenderMaterial / ... | 文件名用聚类语义，不必等于任何一个类型 |
| 实现 / 辅助 / 值类型 | `phase_item.h` ↔ `PhaseItem`、`view_visible_list.h` | 描述性 snake_case |

**规则要点**：
- **禁止 `PascalCase` 文件名**（如 `Camera.h`、`RenderExecuteSystem.h` 一律写成 `camera.h`、`render_execute_system.h`）。
- **文件名 ≠ `class` 名**是允许的（聚类文件），但文件名必须是 snake_case。
- **.h / .cpp 同名配对**：`scene_loader.h` ↔ `scene_loader.cpp`；非单类型文件的 .h 与 .cpp 也保持同一 basename。
- **一个关注点的类型不要塞进别人的文件**：当文件里出现与主类型无关、但自成体系的类型时（如从 `scene_loader.h` 拆出的 `MaterialTextureBackfillSystem`），把它拆到独立文件 `material_texture_backfill_system.h` / `.cpp`。
- 重命名文件时要同步更新所有 `#include` 引用；源文件发现由 CMake 自动完成，无需改动 CMakeLists（除非显式列了 `SOURCES`）。

**自检口诀**：看到大写字母开头的 `.h`/`.cpp` 文件名就停下来想——这应该是蛇形命名；看到 `.h` 里躺着那个 `.cpp` 里才该有的、另一个关注点的类，就想拆。

## 反例墙

| 违规写法 | 正确写法 |
|----------|----------|
| `m_writeQueue` | `m_write_queue` |
| `m_write_queue_` | `m_write_queue` |
| `writeQueue_` | `m_write_queue` |
| `m_FileStream` | `m_file_stream` |
| `fileStream` | `m_file_stream` |
| `kMaxHistory` | `MAX_HISTORY` |
| `DEBUG` / `INFO`（枚举值全大写） | `Debug` / `Info` |
| `struct log_entry` | `struct LogEntry` |
| `void process_batch()` | `void processBatch()` |

## 语言与编码

- **注释语言**：所有代码注释必须使用英文（含 TODO / FIXME / NOTE）。Markdown / 设计文档可用中文。
- **Commit Message**：必须使用英文，遵循 [Conventional Commits](https://www.conventionalcommits.org/) 规范：`type[scope]: description`
- **禁止**：Commit message 中不得包含 `Co-Authored-By:`、`Signed-off-by:` 等自动化签名尾注，也不得包含前导/尾随 `@` 符号。项目只由实际开发者署名，不由工具/AI 代理追加。
- **文件编码**：UTF-8（项目 CMake 已配置 MSVC `/utf-8` 编译选项，无需 BOM）
- **换行符**：C++ / CMake / Python / JSON / Markdown 等统一使用 LF；仅 `.bat` / `.cmd` 使用 CRLF

## 注释规范

代码注释只承担两类职责：**解释代码**（说明一段逻辑为什么这样写、如何处理边界情况）与**备注**（TODO / FIXME / NOTE 等提醒）。注释必须与具体落地代码强绑定，禁止把执行计划、排期、项目节点写进代码注释。

### 禁止行为

1. **禁止在代码注释里引用计划节点 / 编号**
   - ❌ `// Phase 5c: only the first sky settings component is extracted`
   - ❌ `// Lit PBR shader pair (Phase 5a, D1/D8)`
   - ❌ `// TODO(Phase 3+): Remove once all internal relative includes are eliminated`
   - ✅ 只保留对代码本身的解释：`// Only the first sky settings component is extracted`
   - ✅ `// TODO: Remove once all internal relative includes are eliminated`

2. **禁止用 `Phase X` / `Checkpoint Y` / `D1` / `D5` / `Batch A` 等作阶段标注前缀词**
   - 这类标识把注释和某个外部执行计划耦合在一起，计划一旦调整/拆解，注释就会失准或误导阅读者。
   - 若是算法内的顺序步骤（如 "先标记脏树 → 再传播变换"），改用 `Step 1` / `Step 2` 表达执行顺序，而不是 `Phase`。

3. **计划信息不放代码里，放文档里**
   - 里程碑、排期、目标、进度 → 写进设计文档或 `TODO.md`（见下文"技术债务记录"），不要在源文件注释里重复。

### 自检口诀

> 注释只回答两件事：**"这行/这段代码为什么这么写？"** 和 **"这里有什么做法需要留意？"**——不回答 **"这在计划第几步？"**。

选择标准：如果某条注释删掉后，代码阅读者不依赖任何外部计划文档仍能完全理解，那就是合格的注释；反之若注释把理解绑定在一个编号上（Phase/D/Checkpoint），就要改写或删除。

## 自动化检查

项目配置了两个 git hook 自动检查规范：

| Hook | 脚本 | 触发时机 |
|------|------|----------|
| `pre-commit` | `scripts/tools/lint.py --staged` | `git commit` 前，检查 staged C++ 文件风格 |
| `commit-msg` | `scripts/tools/check_commit_msg.py` | `git commit` 后、提交前，检查 commit message 格式 |

完整规则表、排除规则与运行方式见 [LINT_RULES.md](scripts/tools/LINT_RULES.md)。

常用命令：

```bash
# 检查所有源文件
python scripts/tools/lint.py

# 仅检查 git staged 文件
python scripts/tools/lint.py --staged

# 自动修复换行符
python scripts/tools/lint.py --fix
```

## 技术债务记录（`TODO.md`）

- **必须按模块归类**（一级标题），如 `## ECS / Core Runtime`、`## Render / RHI`。
- **单条格式**（不可拆成多行主条目）：
  ```
  - [ ] 模块/子系统 | 文件/位置：具体问题，改进方向
  ```
- **三要素缺一不可**：位置（源文件路径）、问题（具体缺陷）、方向（改进思路）。
- **禁止**：使用 `Phase X` / `Checkpoint Y` 等计划节点前缀；不写文件位置；只有问题没有方向。

**示例**：
- ❌ `Render Checkpoint 2+ | BinnedRenderPhase 退化为线性查找`
- ❌ `Render / RHI | BinnedRenderPhase 性能差`
- ✅ `Render / RHI | BinnedRenderPhase（render/queue/BinnedRenderPhase.h）使用 DynamicArray 线性分箱替代 HashMap，大规模场景下退化为 O(n)，需恢复 HashMap 或引入独立 Resource 系统`
