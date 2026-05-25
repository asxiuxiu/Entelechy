# Entelechy — Agent 快速入口

> 如果你是第一次参与这个项目，请花 30 秒浏览本文件。它能帮你避免 90% 的编码和构建问题。

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
│   └── skills/
│       └── vault-context/
├── .vscode/                ← VS Code 工作区配置
│   ├── c_cpp_properties.json
│   ├── launch.json
│   └── tasks.json
├── AGENTS.md               ← 你在这里（根索引）
├── AGENTS-BUILD.md         ← 构建系统、模块添加指南与构建陷阱
├── AGENTS-CODE.md          ← 代码规范、命名约定与临时架构提醒
├── CMakeLists.txt          ← 根 CMake 入口
├── CMakeSettings.json      ← CMake 设置
├── CMakeUserPresets.json   ← CMake 用户预设
├── Note_TODO.md            ← 笔记待整理
├── README.md               ← 对外文档
├── TODO.md                 ← 技术债务
├── scripts/                ← 构建与工具脚本
│   ├── build/              ← 构建系统
│   │   └── build.py        ← 跨平台主构建入口（推荐）
│   └── tools/              ← 开发工具
│       ├── clean.py        ← 跨平台清理脚本
│       └── gen_reflection.py ← 反射代码生成器：扫描 REFLECT_COMPONENT 宏并生成序列化辅助代码
├── conanfile.py            ← Conan 依赖管理
├── configs/                ← 构建配置（JSON）
│   ├── full_build.json
│   └── engine_only.json
├── launch/                 ← 构建系统工具链
│   ├── generator.py
│   ├── cmake_projects.json ← 引擎模块清单
│   └── templates/          ← CMake / main.cpp 模板
├── plans/                  ← 路线图
│   └── SelfGameEngine-Roadmap.md
├── _engine/                ← 引擎核心（模拟独立仓库）
│   ├── cmake_projects.json
│   └── source/
│       ├── base/           ← 引擎标准库 + Foundation 基础层：固定宽度类型、平台宏、断言体系、字符串、容器（BaseLib） [AGENTS.md](./_engine/source/base/AGENTS.md)
│       ├── core/           ← ECS 数据层 + App 容器 + Prefab + 序列化（CoreLib） [AGENTS.md](./_engine/source/core/AGENTS.md)
│       ├── motor/          ← 运动系统：MovementSystem（MotorLib） [AGENTS.md](./_engine/source/motor/AGENTS.md)
│       ├── bridge/         ← AgentBridge（BridgeLib） [AGENTS.md](./_engine/source/bridge/AGENTS.md)
│       ├── math/           ← 数学库（MathLib） [AGENTS.md](./_engine/source/math/AGENTS.md)
│       ├── memory/         ← 内存管理（MemoryLib） [AGENTS.md](./_engine/source/memory/AGENTS.md)
│       ├── window/         ← 窗口系统（WindowLib） [AGENTS.md](./_engine/source/window/AGENTS.md)
│       ├── log/            ← 日志系统（LogLib） [AGENTS.md](./_engine/source/log/AGENTS.md)
│       ├── render/         ← 图形后端（RenderLib） [AGENTS.md](./_engine/source/render/AGENTS.md)
│       ├── vfs/            ← 虚拟文件系统（VFSLib） [AGENTS.md](./_engine/source/vfs/AGENTS.md)
│       ├── thread_pool/    ← 工作窃取线程池（ThreadPoolLib） [AGENTS.md](./_engine/source/thread_pool/AGENTS.md)
│       ├── test/           ← 轻量级测试框架（TestFrameworkLib） [AGENTS.md](./_engine/source/test/AGENTS.md)
│       ├── test_runner/    ← 统一测试入口（EntelechyTests） [AGENTS.md](./_engine/source/test_runner/AGENTS.md)
│       └── imgui/          ← ImGui 调试 UI（ImGuiLib） [AGENTS.md](./_engine/source/imgui/AGENTS.md)
└── _game/                  ← 游戏逻辑（模拟独立仓库）
    ├── cmake_projects.json
    └── source/
        └── runtime/        ← 游戏运行时（RuntimeLib） [AGENTS.md](./_game/source/runtime/AGENTS.md)
```

## 子文档索引

按你的当前任务，点击对应文档深入阅读：

- **[AGENTS-BUILD.md](./AGENTS-BUILD.md)** — Source Forest 构建流程、如何添加新模块、链接库注册约定、构建陷阱
- **[AGENTS-CODE.md](./AGENTS-CODE.md)** — 命名空间规范（`Entelechy` / `game`）、命名风格、文件编码（UTF-8 with BOM）

## 外部参考
- 开发路线图：`plans/SelfGameEngine-Roadmap.md`

## Agent 探索路径

每个模块目录下都有 `AGENTS.md`，提供该模块的关键文件、入口点和本地规范，可作为探索起点。

## Agent 工作规则

- **目录结构变更时**：如果新增、删除、重命名目录或文件，或调整了目录的用途说明，**必须同步更新本文件（`AGENTS.md`）中的目录结构代码块及相关描述**，确保文档与实际结构保持一致。
- **模块文档维护（关键）**：
  - 如果修改了某模块的代码（类、函数、文件、架构决策），**必须同步检查并更新该模块目录下的 `AGENTS.md`**，确保文件列表、职责描述、重要入口、依赖关系与实际代码保持一致。
  - 如果新增了一个模块目录，**必须为该模块创建 `AGENTS.md`**，可用 `python scripts/tools/generate_module_docs.py` 生成初始模板后再人工补充。
  - 如果迁移或删除了模块目录，**必须同步迁移或删除对应的 `AGENTS.md`**，并检查根 `AGENTS.md` 中的链接。
- **跨模块引用约定**：在 `AGENTS.md` 中引用其他模块时，使用 `[模块名](相对路径)` 的格式，例如 `[Window 模块](../window/AGENTS.md)`。禁止写死具体的类名/函数名（如 `GlfwWindow::create`），避免被引用模块重构后链接失效。
- **遇到不懂的概念时**：如果在代码、文档或讨论中遇到不理解的专业概念、术语或知识点，**先尝试通过 `vault-context` skill 在用户的 Obsidian 知识库中搜索相关笔记**；若知识库中找不到对应内容，再在根目录的 `Note_TODO.md` 中追加一条记录，方便用户后续整理笔记。
- **代码改动后的构建测试**：Agent 在完成任何代码改动后，**必须以 Debug 配置为主进行目标构建测试**，确认改动不会引入编译错误或链接失败。仅在用户明确要求或场景特定需要时，才使用 Release 配置进行验证。Debug 构建命令参考 `AGENTS-BUILD.md`。
- **基础设施不满足要求时必须停下来确认**：如果在实现过程中发现现有代码、工具链、依赖库或设计方案无法满足功能需求或正确性要求（例如移植算法时遇到语言级内存模型差异、需要引入新的外部依赖、或发现底层并发bug），**必须立即暂停编码，向用户汇报问题根因、现有笔记是否覆盖、以及可选的应对方案，等待用户确认后再继续**。禁止在未经确认的情况下自行采用绕过方案或过度复杂化的修复。
- **知识库缺陷发现与完善工作流**：在参考知识库 `SelfGameEngine` 笔记进行实现时，如果发现笔记内容存在缺陷、不全或关键问题未考虑清楚，**必须立即暂停项目编码工作**，按以下流程处理：
  1. **调用 `self-game-engine` skill** 深入学习 UE 和 Bevy 的对应实现，搜索互联网验证行业最佳实践；
  2. **完善或修正知识库中的原笔记**，补充缺失的分析分支、修复错误的代码示例、或添加新的决策依据；
  3. **将完善后的结论带回项目**，重新规划实现方案后再继续编码。
  4. 只有在实现方案**无法从已完善的笔记中明确抉择**时，才回到用户处请求确认。
