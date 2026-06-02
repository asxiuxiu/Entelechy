# Lint 规则

项目通过 `lint.py` 自动检查代码规范，由 `.git/hooks/pre-commit` 在 `git commit` 前触发。

## 检查项

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

## 手动运行

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

## 排除规则

- **不检查**：`build/`、`scripts/`、Conan 引入的第三方依赖
- **内部第三方模块**（如 `imgui/`）：模块自有 API 不检查，但项目代码调用该模块时须遵守规范

## pre-commit hook

Hook 已预配置在 `.git/hooks/pre-commit`，新 clone 仓库无需额外操作。脚本内容：运行 `python scripts/tools/lint.py --staged`，失败时阻止提交。
