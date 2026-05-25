# Entelechy — Agent 快速入口

## 项目是什么

**Entelechy** 是一个渐进式自研游戏引擎，目前处于 Phase 0（最小可运行 ECS 骨架）。

- **核心目标**：从可编译的 ECS demo 演进到工业级引擎
- **当前状态**：已具备 Source Forest 构建架构、基础 ECS 数据层、CMake 工程化
- **技术栈**：C++20、CMake、Python（构建生成器）

## 开发环境预设

**本项目同时支持 Windows 与 macOS/Linux 开发**。

- **所有平台**：统一使用 `python scripts/build/build.py`（Windows）或 `python3 scripts/build/build.py`（macOS / Linux）
- **编译器预设**：Windows 为 MSVC（或 MinGW），macOS / Linux 为 Clang / GCC
- **Agent 在给出命令时**：统一使用 Python 入口，不再区分平台脚本

## 目录结构

```
Entelechy/
├── .agents/                ← Agent skills
├── .vscode/                ← VS Code 工作区配置
├── AGENTS.md               ← 你在这里（根索引）
├── AGENTS-BUILD.md         ← 构建系统、模块添加指南与构建陷阱
├── AGENTS-CODE.md          ← 代码规范、命名约定与文档规范
├── CMakeLists.txt          ← 根 CMake 入口
├── scripts/                ← 构建与工具脚本
│   ├── build/              ← 构建系统
│   └── tools/              ← 开发工具
├── conanfile.py            ← Conan 依赖管理
├── configs/                ← 构建配置（JSON）
├── launch/                 ← 构建系统工具链
│   └── templates/          ← CMake / main.cpp 模板
├── plans/                  ← 路线图
├── _engine/                ← 引擎核心（模拟独立仓库）
│   ├── cmake_projects.json
│   └── source/             ← 引擎模块（各模块含独立 AGENTS.md）
│       ├── base/           ← 引擎标准库 + Foundation 基础层（BaseLib）
│       ├── core/           ← ECS 数据层 + App 容器 + 序列化（CoreLib）
│       ├── motor/          ← 运动系统（MotorLib）
│       ├── bridge/         ← AgentBridge（BridgeLib）
│       ├── math/           ← 数学库（MathLib）
│       ├── memory/         ← 内存管理（MemoryLib）
│       ├── window/         ← 窗口系统（WindowLib）
│       ├── log/            ← 日志系统（LogLib）
│       ├── render/         ← 图形后端（RenderLib）
│       ├── vfs/            ← 虚拟文件系统（VFSLib）
│       ├── asset/          ← 资源管理：Handle Table、异步加载、引用计数（AssetLib）
│       ├── thread_pool/    ← 工作窃取线程池（ThreadPoolLib）
│       ├── test/           ← 轻量级测试框架（TestFrameworkLib）
│       ├── test_runner/    ← 统一测试入口（EntelechyTests）
│       └── imgui/          ← ImGui 调试 UI（ImGuiLib）
└── _game/                  ← 游戏逻辑（模拟独立仓库）
    ├── cmake_projects.json
    └── source/
        └── runtime/        ← 游戏运行时（RuntimeLib）
```

## 子文档索引

按你的当前任务，点击对应文档深入阅读：

- **[AGENTS-BUILD.md](./AGENTS-BUILD.md)** — Source Forest 构建流程、如何添加新模块、链接库注册约定、构建陷阱
- **[AGENTS-CODE.md](./AGENTS-CODE.md)** — 命名空间规范、命名风格、文件编码、技术债务记录格式等

## 外部参考

- 开发路线图：`plans/SelfGameEngine-Roadmap.md`

## Agent 探索路径

每个模块目录下都有 `AGENTS.md`，提供该模块的关键文件、入口点和本地规范，可作为探索起点。

## Agent 工作规则

- **文档同步**：目录结构、模块增删等任何变更，**必须同步更新对应 `AGENTS.md`**（含本文件及模块级文档）。新增模块需新建 `AGENTS.md`。
- **跨模块引用**：引用其他模块时使用 `[模块名](相对路径)` 格式，禁止硬编码类名/函数名。
- **知识库优先**：先在 Obsidian 知识库通过 `vault-context` skill 搜索未知概念；若发现缺陷或关键问题未明，用 `self-game-engine` skill 学习 UE/Bevy 实现并搜索行业实践，完善知识库后再继续。仅当完善后仍无法抉择时，再请求用户确认；找不到则在 `Note_TODO.md` 中追加记录。
- **构建验证**：代码改动后**必须以 Debug 配置构建测试**，确认无编译/链接错误。仅用户明确要求时用 Release。
- **遇阻即停**：若现有代码、工具链、依赖或设计方案无法满足需求，**立即停止编码，向用户汇报根因和已有信息，等待讨论确认**。禁止自行换方案、采用绕过手段或执行奇怪操作。
- **事后审视与技术债务**：每次编码完成后，检查性能热点、架构耦合、边界条件、可维护性、代码一致性。发现债务立即写入 `TODO.md`，条目须按模块归类，包含**位置、问题、方向**三要素。详细格式见 [AGENTS-CODE.md](./AGENTS-CODE.md)。
