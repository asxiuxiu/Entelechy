---
name: vault-context
description: 当用户请求参考 Obsidian 知识库、使用 vault 中的笔记、或在 vibe coding 时需要引入项目本地知识库中的架构/源码分析知识时，读取此 skill 并主动搜索/读取相关笔记。
---

# Vault 上下文注入指南

## 1. 解析配置

每次搜索前，先运行脚本获取 Vault 路径和名称：

```powershell
python .agents/skills/vault-context/scripts/resolve_vault.py
```

输出 JSON：`{"path": "...", "vault_name": "..."}`。优先级：
1. 读取项目根目录 `.vault-context.json`
2. 回退默认值：`D:\asxiuxiu_obsidian_repo` / `asxiuxiu_obsidian_repo`

下文所有 `{{VAULT_PATH}}` 和 `{{VAULT_NAME}}` 均替换为脚本返回的实际值。

## 2. 目录结构

| 目录 | 用途 |
|------|------|
| `Notes/计算机图形学/` | 渲染、PBR、延迟渲染等 |
| `Notes/游戏引擎/` | ECS、反射、内存管理、资源管线等 |
| `Notes/C++编程/` | 模板元编程、并发、构建系统、设计模式等 |
| `Notes/数学基础/` | 线性代数、四元数、碰撞检测等 |
| `Notes/人工智能/` | 行为树、状态机、寻路等 |
| `Notes/构建系统/` | CMake、编译链接等 |
| `Notes/索引/` | 各类 `索引.md` 入口 |
| `Game/` | chaos、proven_ground、wolfgang 等保密源码分析 |
| `workspace/` | C++ 实验代码、CMake 模板等 |

## 3. 何时主动读取

用户发言只要涉及以下任一意图，**无需用户手动粘贴**，立即主动搜索/读取：
- "参考我知识库里的 xxx"
- "根据我笔记里的 xxx 方案来设计"
- "vibe coding 这个模块，和知识库里的 xxx 对照一下"
- "帮我看看这个实现和笔记里记录的有什么区别"
- "按照 vault 里总结的模式来写"

## 4. 搜索策略

**Obsidian 在运行时，优先用 obsidian-cli：**

```powershell
obsidian vault="{{VAULT_NAME}}" search query="关键词" limit=10
obsidian vault="{{VAULT_NAME}}" read path="Notes/游戏引擎/ECS-源码解析"
```

**obsidian-cli 不可用时，立即回退到文件系统：**

```powershell
Grep(pattern: "关键词", path: "{{VAULT_PATH}}\\Notes\\游戏引擎")
Glob(pattern: "Notes/*/索引.md", directory: "{{VAULT_PATH}}")
ReadFile(path: "{{VAULT_PATH}}\\Notes\\游戏引擎\\ECS-源码解析.md")
```

**流程**：搜索 → 挑选 1-3 篇最相关笔记 → 读取 → 提取核心概念/设计模式/代码示例 → 应用到当前任务。

## 5. 关于 /add-dir

若用户准备多轮深度 vibe coding，可建议：
> 执行 `/add-dir {{VAULT_PATH}}` 把知识库挂载到当前工作区，方便后续自动搜索。

不要强制要求。

## 6. 引用规范

- Wikilink（如 `[[索引|xxx索引]]`）在回答里直接用文字引用。
- `Game/` 目录下的保密源码分析，**只提取通用工程原理**，不透露项目专属业务逻辑或敏感数据。
