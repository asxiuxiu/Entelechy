# 日志系统实施计划（LogSystem Implementation Plan）

> **前置依赖**：窗口系统已就绪（`Hello-Engine-Window` 阶段完成）  
> **目标**：从最小可用实现逐步演进到工业级日志基础设施  
> **参考知识源**：`可视化日志系统.md` / `Bevy-bevy_log 源码解析` / `UE-Core 日志源码解析`

---

## ✅ 已完成（当前状态）

以下功能已实现并可用，从计划中移除详细 Task，仅作状态记录：

| 阶段 | 内容 | 状态 |
|------|------|------|
| 阶段 0 | 核心数据结构（LogLevel / LogEntry / LogCategory / StringId） | ✅ |
| 阶段 0 | 异步双缓冲队列 Logger（mutex + write/read queue swap） | ✅ |
| 阶段 0 | 编译期过滤宏体系（LOG_DEBUG / INFO / WARN / ERROR + if constexpr） | ✅ |
| 阶段 0 | 运行时原子级别过滤（std::atomic<LogLevel> + setMinLevel） | ✅ |
| 阶段 0 | 双端输出（控制台 + 文件） | ✅ |
| 阶段 0 | 接入构建系统（LogLib 已注册） | ✅ |
| 阶段 0 | CMake 注入 `SHIPPING_BUILD`（`--shipping` 参数） | ✅ |
| 阶段 2 | 输出设备抽象 `LogOutputDevice`（Console / File / JsonFile） | ✅ |
| 阶段 2 | JSON Lines 输出（`logs/engine.jsonl`） | ✅ |
| 阶段 2 | 文件滚动（10MB 阈值，保留最近 5 个备份） | ✅ |
| 阶段 2 | Once 宏家族（LOG_XXX_ONCE） | ✅ |

> **注意**：`core` 模块（`world.cpp`）因循环依赖限制（LogLib 依赖 CoreLib），暂时保留 `printf`。待后续提取更底层基础库后再迁移。

---

## 阶段 1：可视化与过滤 —— 让日志在窗口里看得见

**目标**：基于 ImGui 实现一个带级别过滤、自动滚动的日志面板；支持按分类开关。

**状态**：⏸️ **阻塞中** —— 当前无 RHI / 渲染后端，ImGui 面板暂时无法实现。

**验收标准**：
- [ ] 窗口内有一个"Logs"面板，实时显示最近日志
- [ ] 可以勾选/取消某分类（如 Render / Physics）的显示
- [ ] 可以下拉选择最低显示级别
- [ ] 不同级别有不同颜色（Error=红，Warning=黄，Info=白，Debug=灰）

### Task 1.1 ImGui 日志面板
- 在现有 ImGui 渲染循环中新增 `drawLogPanel()`
- 遍历 `logger.history()`，用 `ImGui::TextColored` 输出
- 自动滚动到底部（当用户没有手动上滚时）
- 面板支持 `ImGui::Begin("Logs")` 独立窗口或嵌入主界面

### Task 1.2 分类系统
- `category` 从字符串升级到 `StringId` 或固定枚举（如 `LogCategory::Render`）
- 在 `LogEntry` 中保留 `category_hash`
- 日志宏增加分类参数（如 `LOG_INFO("Render", "msg")`）

### Task 1.3 运行时分类过滤面板
- `std::unordered_map<uint32_t, bool>` 存储各分类的显示开关
- ImGui 面板顶部绘制分类复选框列表
- `shouldShow()` 函数先过级别、再过分类

### Task 1.4 面板交互优化
- 支持文本搜索过滤（按关键词过滤消息内容）
- 支持 Clear 按钮清空历史
- 支持 Pause 按钮暂停自动刷新

---

## 阶段 2：设备抽象与结构化 —— 好用、可扩展、AI 友好

**状态**：部分已完成（设备抽象、JSONL、滚动、Once 宏已实现）。以下为剩余未实现内容。

### Task 2.5 AgentBridge 结构化接口（可选，看 ECS 进度）
- 提供 `log.query(category, level, max_count) -> LogEntry[]`
- 提供 `log.set_level(category, level) -> bool`
- 接口输出 JSON，供 AI 直接消费 `.jsonl` 日志流

---

## 阶段 3：工业级优化 —— 高性能、诊断联动、崩溃保护

**目标**：当高频日志成为瓶颈时升级架构；与 ECS/Event 系统深度融合；确保崩溃不丢日志。

**状态**：⏸️ **长期规划** —— 当前日志量不大，阶段 0~2 已足够。以下任务按需升级。

**验收标准**：
- [ ] 高频日志场景（>1000条/帧）无明显锁竞争开销
- [ ] 诊断数据（FPS、帧时间、实体数）自动注入日志流
- [ ] 崩溃前最后几条日志被强制刷盘

### Task 3.1 TLS 写缓冲（UE TraceLog 模式）
- 每个线程 `thread_local` 预分配 64KB 环形缓冲
- 三指针模型：`Cursor`（写入）/`Committed`（提交）/`Reaped`（回收）
- 后台 Worker 线程定期收集所有 TLS 缓冲区
- 仅在 `Committed` 边界批量输出，消除锁竞争

> **诚实边界**：如果项目只有 2~3 个线程且日志量不大，阶段 2 的双缓冲队列已足够。此任务为可选升级。

### Task 3.2 结构化字段日志（Bevy tracing 思想）
- 引入结构化宏：`LOG_STRUCT("Render", "frame_stats", field("fps", fps), field("draw_calls", dc))`
- JSON 输出中增加 `fields` 对象：`{"fps": 60.0, "draw_calls": 128}`
- AI 可直接按 `fields.fps < 30` 过滤

### Task 3.3 诊断联动
- 定义 `DiagnosticSnapshot`：`fps`, `frame_time_ms`, `entity_count`, `memory_used_mb`
- 每帧自动 `LOG_STRUCT("Diagnostics", "frame_snapshot", ...)`
- AI 查询"为什么掉帧"时可在同一日志流中找到关联

### Task 3.4 崩溃前 Emergency Flush
- 注册 `std::set_terminate` 和信号处理函数
- 崩溃时遍历所有缓冲区的 `Committed` 数据，无锁强制写入文件
- 再调用现有的栈回溯 / minidump 生成

### Task 3.5 ECS Event 驱动重构（长期）
- 当 ECS 骨架稳定后，将日志从 OOP 单例迁移为 ECS Event：
  - `LogEvent`：瞬时 ECS Event
  - `LogSink` Component：每个输出目标是一个 Entity
  - `LogFilter` Resource：全局过滤配置
  - `ConsoleSinkSystem` / `FileSinkSystem` / `JsonSinkSystem`：并行消费
- 移除全局 `g_logger`，所有日志通过 `World` Event 管道

> **迁移时机**：建议在完成「最小 ECS 数据层」和「事件总线」后再执行。

---

## 阶段对照速查表

| 阶段 | 核心能力提升 | 对应笔记章节 | 推荐时机 |
|------|-------------|-------------|---------|
| 阶段 0 | 替代 printf，线程安全，编译期裁剪 | How - 阶段 1 (A~C) | **已完成** |
| 阶段 1 | ImGui 面板，分类过滤，颜色区分 | How - 阶段 1 (D) | ⏸️ 需渲染后端 |
| 阶段 2 | 设备抽象，JSON 输出，文件滚动，Once | How - 阶段 2 (A~E) | **已完成** |
| 阶段 3 | TLS 无锁，结构化字段，诊断联动，崩溃 flush | How - 阶段 3 (A~D) | ⏸️ 性能实测有瓶颈后 |
| 阶段 3.5 | ECS Event 驱动，完全无全局状态 | ECS 映射章节 | ⏸️ ECS 核心稳定后 |

---

## 开发原则与 Checklist

- [x] **始终并行输出**：人类读 `.log`，AI 读 `.jsonl`，从第一天保留双管道
- [x] **编译期零开销优先**：`if constexpr` 过滤比运行时过滤更重要
- [x] **Shipping 无 Debug**：发布构建中 `Debug`/`Info` 日志宏展开为空
- [x] **先能用再好用**：阶段 0~1 是刚需，阶段 2~3 按需升级
- [ ] **OOP → ECS 渐进迁移**：阶段 0~2 用单例快速验证，阶段 3.5 再重构
