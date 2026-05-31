# 代码规范

## 基础库优先

项目已封装的基础库（`core/` 模块）必须优先使用，禁止在基础库已覆盖的领域重复引入 STL 或自行实现：

| 基础库设施 | 替代目标 | 说明 |
|---|---|---|
| `DynamicArray<T>` | `std::vector<T>` | 带可插拔分配器的动态数组 |
| `HashMap<K,V>` / `HashSet<K>` | `std::unordered_map` / `std::unordered_set` | 开放寻址哈希表 |
| `SmallString` | `std::string` | SSO 小字符串 |
| `formatString` | `std::format` / `snprintf` | 零堆分配格式化 |
| `Vec2/Vec3/Vec4/Mat4/Quat` 等 | `glm::` | 数学库 |
| `DefaultAllocator` / `IAllocator` | 裸 `new/delete` / `malloc/free` | 对齐分配器 |
| `ObjectPool<T>` | 裸 `new/delete`（对象池场景） | 带代际安全检查的对象池 |
| `FrameArena` | 临时/帧级堆分配 | O(1) 重置的帧分配器 |

**底线**：基础库已有对应实现时，不允许以"习惯""省事"为由绕回 STL 或裸分配。若基础库接口确实不满足需求，先讨论扩展基础库，而非在业务代码里开例外。

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
- **文件编码**：UTF-8（项目 CMake 已配置 MSVC `/utf-8` 编译选项，无需 BOM）
- **换行符**：C++ / CMake / Python / JSON / Markdown 等统一使用 LF；仅 `.bat` / `.cmd` 使用 CRLF

## 自动化检查

项目配置了 `scripts/tools/lint.py` 自动检查代码规范，通过 git 原生 `.git/hooks/pre-commit` 在 `git commit` 前触发。

### 检查项

| 规则 | 说明 | 严重程度 |
|------|------|----------|
| `naming.member-variable` | 成员变量须为 `m_snake_case` | error |
| `naming.function` | 函数须为 `camelCase` | error |
| `naming.class` | 类/结构体须为 `PascalCase` | error |
| `assert.bare` | 禁止裸 `assert()`，须用 `CHECK/VERIFY/ENSURE` | error |
| `language.chinese-comment` | 代码注释须为英文 | warning |
| `format.line-ending` | 须使用 LF 换行符 | warning |
| `format.encoding` | 须使用有效 UTF-8 编码 | warning |
| `cmake.relative-path` | `CMakeLists.txt` 须用 `${CMAKE_CURRENT_LIST_DIR}` | error |

### 手动运行

```bash
# 检查所有源文件
python scripts/tools/lint.py

# 仅检查 git staged 文件（pre-commit 场景）
python scripts/tools/lint.py --staged

# 自动修复换行符（CRLF → LF）
python scripts/tools/lint.py --fix

# 检查指定文件/目录
python scripts/tools/lint.py _engine/source/core/
```

### 排除规则

- **不检查**：`build/`、`scripts/`、Conan 引入的第三方依赖
- **内部第三方模块**（如 `imgui/`）：模块自有 API 不检查，但项目代码调用该模块时须遵守规范

### 安装 pre-commit hook

```bash
# 复制 hook 到 git hooks 目录（已预配置，新 clone 仓库需执行一次）
cp .git/hooks/pre-commit.sample .git/hooks/pre-commit  # 如不存在
# 然后编辑 .git/hooks/pre-commit 加入 lint 调用
```

实际 hook 脚本已写入 `.git/hooks/pre-commit`：

```sh
#!/bin/sh
cd "$(dirname "$0")/../.." || exit 1
python scripts/tools/lint.py --staged
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
    echo "[pre-commit] Lint checks failed. Fix violations before committing."
    exit 1
fi
exit 0
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
