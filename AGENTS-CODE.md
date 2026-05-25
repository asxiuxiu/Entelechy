# Entelechy — 代码规范

## 命名空间规范（参考 CHAOS 引擎）

根据知识库中 CHAOS 引擎的源码分析，工业级引擎的命名空间遵循以下模式：

- **引擎根命名空间 = 引擎名**：CHAOS 使用 `namespace Chaos`，所有引擎核心代码（`base/`、`mempool/`、`client/`、`server/`、`editor/`）均在此命名空间下。
- **子模块用嵌套命名空间**：如 `Chaos::MemoryPool`。
- **游戏项目使用独立命名空间**：proven_ground 使用 `namespace Xenon { using namespace Chaos; }`，在保持游戏自身边界的同时无缝引入引擎能力。

**本项目约定**：
- **引擎层**：`namespace Entelechy`（大驼峰，与引擎同名）
- **游戏层**：`namespace game`（当前为占位；若后续有具体游戏代号，建议仿照 CHAOS 改用独立大驼峰命名空间，如 `namespace Xenon`）

---

## 代码规范

### 命名空间

- **引擎层**：`namespace Entelechy { ... }`（与引擎同名，大驼峰，参考 CHAOS 的 `Chaos`）
- **游戏层**：`namespace game { ... }`（当前为占位，后续具体游戏项目可改用独立代号如 `Xenon`）
- 不要再用 `Engine`、`Game` 大驼峰形式

### 断言与检查宏（`foundation_types.h`）

项目中提供五层断言/检查宏，**禁止混用 `assert()` 或自行手写 `if (!cond) abort()`**，必须按语义选择下表：

| 宏 | 语义 | Debug 行为 | Release 行为 | 正确用法 |
|---|---|---|---|---|
| `CHECK(cond)` | 内部不变量 / 前置条件 | 失败 → 打印 + `abort()` | **完全消除**（`cond` 不执行，零开销） | 纯断言，无副作用。例如 `CHECK(index < m_count)` |
| `VERIFY(cond)` | 必须执行的表达式 + Debug 断言 | 失败 → 打印 + `abort()` | **执行 `cond`，不检查** | `cond` 有副作用，Release 下仍需执行。例如 `VERIFY(ptr = allocate())` |
| `ENSURE(cond)` | 非致命错误检查 | 失败 → 打印日志，继续运行 | **执行 `cond`，不检查** | 可恢复的错误场景，记录但不崩溃。例如 `ENSURE(file.isOpen())` |
| `ENSURE_MSG(cond, fmt, ...)` | 带自定义消息的非致命检查 | 同上 | 同上 | 需要额外上下文时，例如 `ENSURE_MSG(ret >= 0, "bind failed: %d", ret)` |
| `STATIC_ASSERT(cond, msg)` | 编译期断言 | 编译失败 | 编译失败 | 类型大小、模板约束、常量表达式。例如 `STATIC_ASSERT(sizeof(Vec4) == 16, "Vec4 must be 16 bytes")` |

**⚠️ 选择口诀**：
- 条件**无副作用** + 严重内部错误 → **`CHECK`**
- 条件**有副作用**（如赋值、分配）+ 严重错误 → **`VERIFY`**
- 错误**可恢复**、仅需记录 → **`ENSURE` / `ENSURE_MSG`**
- 检查**编译期已知**的常量/类型 → **`STATIC_ASSERT`**

**常见陷阱**：
- ❌ `CHECK(ptr = malloc(size))` — Release 下赋值消失，内存泄漏
- ✅ `VERIFY(ptr = malloc(size))` — Release 下赋值保留，仅去除断言
- ❌ `ENSURE(index < m_count)` — 数组越界是致命错误，应使用 `CHECK`

---

### 命名风格

| 类型 | 风格 | 示例 |
|------|------|------|
| 函数（含成员函数、自由函数） | `camelCase` | `initCore()`, `entityCount()`, `registerSystem()` |
| 类/结构体 | `PascalCase` | `World`, `AgentBridge`, `Scheduler` |
| 枚举值（enum values） | `PascalCase` | `Debug`, `KeyPress`, `MouseMove` |
| 成员变量（无论 public/private） | `m_snake_case` | `m_free_list`, `m_next_id`, `m_positions` |
| 局部变量 / 参数 | `camelCase`（无 `m_` 前缀） | `dt`, `entityCount` |
| 宏/常量 | `UPPER_SNAKE_CASE` | `ENTITY_MAX` |

> ⚠️ **命名风格是本项目最高优先级的硬约束**。Agent 在生成任何标识符前，**必须先查上表确认风格**，禁止凭训练惯性直接输出。写完后必须逐行回顾所有新引入的标识符，确保无一处违规。

### ❌ 常见陷阱（反例墙）

以下是在其他 C++ 项目中常见、但**在本项目严格禁止**的写法。Agent 若产生以下任何 pattern，视为必须立即修正的 bug：

| 违规写法 | 问题 | 正确写法 |
|----------|------|----------|
| `m_writeQueue` | 成员变量用了 camelCase（Google/Unreal 惯性） | `m_write_queue` |
| `m_write_queue_` | 尾部下划线（LLVM/libc++ 惯性） | `m_write_queue` |
| `writeQueue_` | 尾部下划线无 `m_` 前缀 | `m_write_queue` |
| `m_FileStream` | `m_` 后首字母大写（Microsoft 惯性） | `m_file_stream` |
| `fileStream` | 成员变量无前缀 | `m_file_stream` |
| `kMaxHistory` | 常量用了 `kCamelCase`（Google 惯性） | `MAX_HISTORY` |
| `DEBUG` / `INFO` | 枚举值全大写 | `Debug` / `Info` |
| `struct log_entry` | 结构体 snake_case | `struct LogEntry` |
| `void process_batch()` | 函数 PascalCase | `void processBatch()` |

**自检口诀**：`m_` 之后全小写加下划线；函数首单词全小写；类名每个单词首字母大写；枚举值每个单词首字母大写；宏全部大写。

### 注释语言

**所有代码注释必须使用英文**。包括：
- 文件头注释（file header comments）
- 行内注释（inline comments）
- 文档注释（documentation comments, e.g. `///` or `//`）
- TODO / FIXME / NOTE 等标记注释

This rule applies to all source files under `_engine/` and `_game/`.
Markdown / design docs / plans may still use Chinese for human readability.

### Commit Message 语言

**所有 Git Commit Message 必须使用英文**。包括：
- 标题（subject line）
- 正文（body）
- 页脚（footer，如 `Closes #123` 等）

遵循 [Conventional Commits](https://www.conventionalcommits.org/) 规范：
```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

### 文件编码

**所有文本文件必须保存为 UTF-8 with BOM**。这是硬性要求，否则：
- MSVC 编译中文注释会报 `C4819`
- README/文档在 Windows 下显示乱码

如果你用 VS Code，可以在设置中启用：
```json
"files.encoding": "utf8bom"
```

### 换行符规范

为兼顾跨平台协作与 Windows 工具链兼容性，项目统一规定如下：

| 文件类型 | 换行符 | 说明 |
|----------|--------|------|
| C++ 源码（`.h` / `.hpp` / `.c` / `.cpp`） | **LF** | 跨平台标准，MSVC/CMake 均兼容 |
| CMake 文件（`CMakeLists.txt` / `.cmake`） | **LF** | CMake 原生偏好 LF |
| Python / JSON / Markdown / YAML / TOML / INI | **LF** | 统一规范，避免混用 |
| Windows 批处理（`.bat` / `.cmd`） | **CRLF** | Windows 命令解释器兼容性要求 |
| 图片 / 二进制库（`.png`、`.dll` 等） | **N/A** | 标记为 `binary`，Git 不处理换行 |

**仓库中的文件必须严格遵循 `.gitattributes` 中的 `eol` 设定**。提交前若发现混用，执行以下命令自动统一：

```powershell
git add --renormalize .
```

> 开发者在 Windows 下使用 VS Code 时，建议设置 `"files.eol": "\n"`，以确保新创建的源码文件默认使用 LF。

---

## 技术债务记录规范（`TODO.md`）

- **必须按模块归类**（一级标题），如 `## ECS / Core Runtime`、`## Render / RHI`。
- **单条格式**（不可拆成多行主条目）：
  ```
  - [ ] 模块/子系统 | 文件/位置：具体问题，改进方向
  ```
- **三要素缺一不可**：
  1. **位置**：涉及的源文件路径或代码位置（如 `render/queue/BinnedRenderPhase.h`）。
  2. **问题**：具体缺陷或技术债务，禁止空泛描述（如"需要优化"）。
  3. **方向**：未来改进的明确思路或目标状态。
- **禁止项**：
  - ❌ 使用 `Phase X`、`Checkpoint Y`、`Milestone Z` 等计划节点作为前缀。
  - ❌ 不写文件位置或只写模块名（如"Render 需要重构"）。
  - ❌ 只有问题没有方向（如"BinnedRenderPhase 性能差"）。
- **示例**：
  - ❌ `Render Checkpoint 2+ | BinnedRenderPhase 退化为线性查找`
  - ❌ `Render / RHI | BinnedRenderPhase 性能差`
  - ✅ `Render / RHI | BinnedRenderPhase（render/queue/BinnedRenderPhase.h）使用 DynamicArray 线性分箱替代 HashMap，大规模场景下退化为 O(n)，需恢复 HashMap 或引入独立 Resource 系统`
