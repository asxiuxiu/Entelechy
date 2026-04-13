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

### 命名风格

| 类型 | 风格 | 示例 |
|------|------|------|
| 函数（含成员函数、自由函数） | `camelCase` | `initCore()`, `entityCount()`, `registerSystem()` |
| 类/结构体 | `PascalCase` | `World`, `AgentBridge`, `Scheduler` |
| 成员变量（无论 public/private） | `m_snake_case` | `m_free_list`, `m_next_id`, `m_positions` |
| 局部变量 / 参数 | `camelCase`（无 `m_` 前缀） | `dt`, `entityCount` |
| 宏/常量 | `UPPER_SNAKE_CASE` | `ENTITY_MAX` |

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

