# Entelechy — Claude Code 规范入口

Claude Code 在处理本项目时，**必须**遵循以下文档加载规则。

## 启动时必读（每次会话）

1. [AGENTS.md](AGENTS.md) — 项目概览、开发环境、Agent 工作规则
2. [AGENTS-BUILD.md](AGENTS-BUILD.md) — 构建系统规范
3. [AGENTS-CODE.md](AGENTS-CODE.md) — 代码规范、命名约定

## 模块级规范（按需读取）

处理任何模块代码前，先检查该模块目录下是否有 `AGENTS.md`，有则必读。模块规范优先于根级规范。

模块规范文件遵循统一路径模式：`<module>/AGENTS.md`

## 关键规则摘要

- 文档同步：目录结构、模块增删等变更，必须同步更新对应 `AGENTS.md`
- 跨模块引用：使用 `[模块名](相对路径)` 格式，禁止硬编码类名/函数名
- 构建验证：代码改动后必须以 Debug 配置构建测试
- 遇阻即停：无法满足需求时立即停止编码，向用户汇报，禁止自行换方案
