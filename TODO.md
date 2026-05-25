# SelfGameEngine TODO / 技术债务

## ECS / Core Runtime
- [ ] **Milestone 0.1** | `src/world.h` : `std::vector<bool> alive` 存在性能陷阱。
  - 原因：`std::vector<bool>` 是 C++ 的位压缩特化容器，读写较慢且无法返回真实引用。
  - 方案：后续替换为 `std::vector<uint8_t>` 或引入 Sparse Set 架构。
- [ ] **Milestone 0.1** | `src/world.h` : `positions` / `velocities` 数组只增不减，存在内存空洞。
  - 场景：先 spawn 大量实体再销毁大部分，数组仍保持峰值大小，浪费内存。
  - 方案：**Milestone 1.2** 升级为 Sparse Set（`dense + sparse`），让存活数据保持紧凑，避免大结构体数组空洞。
- [ ] **Milestone 0.1** | `AgentBridge` 越界承担了 System 注册职责。
  - 位置：`_engine/source/bridge/agent_bridge.cpp:12`
  - 问题：`AgentBridge` 当前直接持有 `World`、`Scheduler`、`MovementSystem`，并在 `init()` 中注册 system，把「AI 接口桥梁」与「运行时装配」耦合在一起。
  - 方案：将 `World`、`Scheduler` 和 system 注册上提到 Runtime / App 层，`AgentBridge` 仅通过指针/引用访问外部注入的世界与调度器，职责边界止于「让 AI 安全读写引擎状态」。

---

## Render / RHI
- [ ] **RHI Phase 1 → Phase 2** | ECS 集成：将 `GLRHIDevice` 包装为 ECS `Resource`。
  - 目标：让 `MeshRenderSystem` 通过 `IRHICommandList` 录制命令，替代直接 GL 裸调用。
  - 参考：`_engine/source/render/rhi_device.h`
- [ ] **RHI Phase 2** | 多线程命令录制预留：引入简化版延迟命令缓冲。
  - 目标：当 Draw Call > 2000 且 Profiling 确认 CPU 瓶颈时，将 `GLCommandList` 的即时执行替换为 `LinearAllocator` + 引擎级命令枚举 + `switch-case` 翻译执行。
  - 关键设计：单线程录制 → 多线程并行生成 `DrawPacket` → 单线程排序 → 单线程翻译。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 2/3
- [ ] **RHI Phase 2** | GPU 资源延迟删除队列。
  - 目标：解决 "CPU 删除纹理但 GPU 还在读" 的 race condition。
  - 方案：引用计数归零后进入 RHI 内部延迟删除队列，帧边界检查 GPU Fence，确认 GPU 完成后批量释放。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 4
- [ ] **RHI Phase 2** | PSO 异步编译骨架填充。
  - 目标：消除运行时缓存未命中造成的帧时间尖刺（hitch）。
  - 方案：`PSOManager::getOrCreateAsync()` 缓存未命中时启动后台线程编译，同时返回占位 PSO（如最简单的纯色着色器）。后台完成后自动切换。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 5
- [ ] **RHI Phase 3** | 启动第二个后端（D3D12 或 Vulkan）。
  - 前提：RHI 抽象接口已跑通带纹理旋转立方体，接口稳定。
  - 策略：先钉死 D3D12（Windows 默认，调试工具顶尖），跑通全部上层管线后再移植 Vulkan。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 7
- [ ] **RHI 远期** | 分离的 Render World（双 World 模型）。
  - 目标：主 World 跑逻辑（30Hz），Render World 跑渲染（60Hz），实现 CPU-GPU 流水线并行、多视图隔离、确定性快照。
  - 方案：每帧 Extract 阶段单向只读复制可渲染数据到 Render World，Render World 内 System 生成命令，最终 `PresentSystem` 输出。
  - 参考：知识库 `Notes/SelfGameEngine/渲染管线与第一帧/RHI抽象层.md` 问题 6

---

## AI / Agent 基建
> 2026-04-13 讨论纪要：AI 不应是独立的第三个 exe，而是「协议 + 桥梁」。先记录为技术债务，后续按里程碑渐进。

- [ ] **MCP 适配层** | 给 `AgentBridge` 套一层标准 MCP Server 壳子。
  - 原因：当前 `AgentBridge` 是直接暴露的内部 API，外部 AI 客户端（Claude Desktop / Kimi CLI / Cursor）无法直接接入。
  - 方案：增加 `McpAdapter`，将 `ToolRegistry` 翻译成 MCP `ListToolsResult` / `CallToolRequest`，使引擎成为标准 MCP Server。
  - 参考：`Notes/Agent/引擎-AI-集成实战-MVP.md` 修正 1。
- [ ] **Command Buffer（命令缓冲）** | AI 的写操作应在 Tick 边界统一提交。
  - 原因：当前 `set_component` 直接修改世界状态，若 AI 在 System tick 中途写入会破坏迭代器稳定性。
  - 方案：引入 `CommandBuffer`，`AgentBridge::set_component` 先 stage，在 `Scheduler::tick()` 结束后统一 `apply()`。
  - 参考：路线图 `Phase 1.4`。
- [ ] **ChangeLog + 快照回滚** | 支撑 Runtime 调试的「后悔药」。
  - 原因：AI 在 Runtime 调参或执行「奇怪行为」时，需要能追踪影响、快速撤销。
  - 方案：`World` 内建 `ChangeLog` 记录 `(entity, component, field, old, new)`；实现 `WorldSnapshot::capture / restore`，在 `AgentBridge` 暴露 `snapshot(name)` / `rollback(name)` 工具。
  - 参考：路线图 `Phase 2.1 / 2.2`。
- [ ] **Permission Gate（权限边界）** | 防止 AI 手滑破坏世界。
  - 原因：只读与写工具目前混在一起，没有统一的安全 gate。
  - 方案：在 `AgentBridge::execute_tool()` 入口处嵌入三级权限（Deny / Ask / Allow），配合组件白名单和 `ApprovalRuntime`；高危操作（如批量删除）强制人工确认，常规写操作自动记录 Undo。
  - 参考：路线图 `Phase 2.5`、`Notes/Agent/Permission-System-架构解析`。
- [ ] **流式事件协议 + Editor UI Bridge** | 让 Editor 能实时看到 AI 在做什么。
  - 原因：当前 `AgentBridge` 是批处理式返回，Editor 无法同步 AI 的「正在思考」「调用了工具 X」等中间状态。
  - 方案：借鉴 Kimi CLI 的 Wire 协议，定义 `EngineDisplayBlock` 事件流（`diff / changelog / task_status`），`AgentBridge` 在工具执行中途 `emitEvent`，Editor AI Panel 消费并渲染。
  - 参考：`Notes/Agent/Agent-Loop-架构解析`、`Notes/Agent/UI-System-架构解析`。
- [ ] **事务沙箱与多 Agent 编排** | 支持多个 AI Agent 并行协作不踩脚。
  - 原因：未来复杂任务（如「搭建一个射击关卡」）需要 Director Agent 拆分子任务给 LevelDesignAgent、GameplayAgent 并行执行。
  - 方案：`TransactionalWorld`（主世界 + 沙箱副本）+ `AgentOrchestrator` 冲突检测与合并；每个 Agent 绑定独立 `ApprovalSource` 和组件锁。
  - 参考：路线图 `Phase 3.6`、`Notes/Agent/Multi-Agent-架构解析`。
