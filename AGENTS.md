# Entelechy — Agent 入口

**Entelechy** 是一个渐进式自研游戏引擎。技术栈：C++20、CMake、Python。

## 构建入口

```bash
# 默认构建（引擎 + 游戏运行时）
python scripts/build/build.py

# Debug（代码改动后的默认测试配置）
python scripts/build/build.py --debug

# Release
python scripts/build/build.py --release
```

产物：可执行文件 → `build/bin/`，静态库 → `build/lib/`。

## 子文档

- **[AGENTS-BUILD.md](AGENTS-BUILD.md)** — 添加模块、构建陷阱
- **[AGENTS-CODE.md](AGENTS-CODE.md)** — 命名规范、编码约束、技术债务格式

各模块目录下的 `AGENTS.md` 提供该模块的入口点和本地规范。

## Agent 工作规则

- **文档同步**：目录结构、模块增删等变更，必须同步更新对应 `AGENTS.md`（含本文件及模块级文档）。新增模块需新建 `AGENTS.md`。
- **跨模块引用**：引用其他模块时使用 `[模块名](相对路径)` 格式，禁止硬编码类名/函数名。
- **知识库优先**：先在 Obsidian 知识库搜索未知概念；若发现缺陷或关键问题未明，用 `self-game-engine` skill 学习 UE/Bevy 实现并搜索行业实践，完善知识库后再继续。仅当完善后仍无法抉择时，再请求用户确认；找不到则在 `Note_TODO.md` 中追加记录。
- **构建验证**：代码改动后必须以 Debug 配置构建测试，确认无编译/链接错误。仅用户明确要求时用 Release。
- **遇阻即停**：若现有代码、工具链、依赖或设计方案无法满足需求，立即停止编码，向用户汇报根因和已有信息，等待讨论确认。禁止自行换方案、采用绕过手段或执行奇怪操作。
- **事后审视与技术债务**：每次编码完成后，对照最初计划检查实现偏差，同时审视性能热点、架构耦合、边界条件、可维护性、代码一致性。发现偏差或债务立即写入 `TODO.md`，格式见 [AGENTS-CODE.md](AGENTS-CODE.md)。
- **代码规范检查**：提交前自动运行 `python scripts/tools/lint.py --staged` 检查命名风格、格式等规范。详见 [AGENTS-CODE.md](AGENTS-CODE.md) 的「自动化检查」一节。
