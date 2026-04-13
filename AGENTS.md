# Entelechy — Agent 快速入口

> 如果你是第一次参与这个项目，请花 30 秒浏览本文件。它能帮你避免 90% 的编码和构建问题。

## 项目是什么

**Entelechy** 是一个渐进式自研游戏引擎，目前处于 Phase 0（最小可运行 ECS 骨架）。

- **核心目标**：从可编译的 ECS demo 演进到工业级引擎
- **当前状态**：已具备 Source Forest 构建架构、基础 ECS 数据层、CMake 工程化
- **技术栈**：C++17、CMake、Python（构建生成器）

## 开发环境预设

**本项目默认在 Windows 环境下开发与构建**。

这意味着：
- 构建脚本、工具链调用、路径风格均按 Windows 约定编写
- Agent 在给出命令时应优先使用 Windows PowerShell / CMD 语法，而非 POSIX Shell
- 编译器预设为 MSVC（或 MinGW on Windows），路径分隔符使用 `\` 或 `/` 均可

## 目录结构

```
Entelechy/
├── AGENTS.md               ← 你在这里（根索引）
├── AGENTS-BUILD.md         ← 构建系统、模块添加指南与构建陷阱
├── AGENTS-CODE.md          ← 代码规范、命名约定与临时架构提醒
├── README.md               ← 对外文档
├── TODO.md                 ← 技术债务
├── plans/                  ← 路线图
│   └── SelfGameEngine-Roadmap.md
├── build.bat               ← 主构建入口
├── configs/                ← 构建配置（JSON）
│   ├── full_build.json
│   └── engine_only.json
├── launch/                 ← 构建系统工具链
│   ├── generator.py
│   ├── cmake_projects.json ← 引擎模块清单
│   └── templates/          ← CMake / main.cpp 模板
├── _engine/                ← 引擎核心（模拟独立仓库）
│   ├── cmake_projects.json
│   └── source/
│       ├── core/           ← ECS 数据层（CoreLib）
│       ├── system/         ← System / Scheduler（SystemLib）
│       └── bridge/         ← AgentBridge（BridgeLib）
└── _game/                  ← 游戏逻辑（模拟独立仓库）
    ├── cmake_projects.json
    └── source/
        └── runtime/        ← 游戏运行时（RuntimeLib）
```

## 子文档索引

按你的当前任务，点击对应文档深入阅读：

- **[AGENTS-BUILD.md](./AGENTS-BUILD.md)** — Source Forest 构建流程、如何添加新模块、链接库注册约定、构建陷阱
- **[AGENTS-CODE.md](./AGENTS-CODE.md)** — 命名空间规范（`Entelechy` / `game`）、命名风格、文件编码（UTF-8 with BOM）

## 外部参考
- 开发路线图：`plans/SelfGameEngine-Roadmap.md`

## Agent 工作规则

- **遇到不懂的概念时**：如果在代码、文档或讨论中遇到不理解的专业概念、术语或知识点，**先尝试通过 `vault-context` skill 在用户的 Obsidian 知识库中搜索相关笔记**；若知识库中找不到对应内容，再在根目录的 `Note_TODO.md` 中追加一条记录，方便用户后续整理笔记。
