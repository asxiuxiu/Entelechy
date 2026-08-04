# Entelechy 待办事项汇总（三份审计计划合并精简版）

> **生成时间**：2026-08-04
> **数据来源**：以下三份计划已全部落实或审计完毕，本文件仅保留**仍需关注**的事项。
> - `plan/string-system-audit-plan.md`（2026-06-01）
> - `plan/module-dependency-analysis.md`（2026-07-07）
> - `plan/ALLOCATOR_ROADMAP.md`（2026-05-31）
>
> **已落实的事项已从本文档移除**，三份原计划归档为历史参考。

---

## 优先级排序

按影响 × 可行性排序：

| # | 优先级 | 事项 | 来源 | 预估 |
|---|--------|------|------|------|
| 1 | **高** | ~~ImGuiLib 解除对 EcsLib 的耦合~~ ✅ | 模块依赖分析 #3 | 已完成 2026-08-04 |
| 2 | **中** | NamePool 迁移为 ECS Resource | 字符串审计 P1 | 2h |
| 3 | **低** | 增加 `entelechy_snprintf` 跨平台封装 | 字符串审计 P2 | 30min |
| 4 | **低** | `FunctionRef` 类型——替代 `std::function` 堆分配 | 分配器路线图 A.7 | 1h |

---

## 1. ImGuiLib 解除对 EcsLib 的耦合 [高] ✅ 已完成 (2026-08-04)

**结果**：创建 `_engine/source/editor/` (EditorLib 模块)，将 ECS 耦合代码迁出 ImGuiLib：
- `buildECSInspector()` + `drawField()` → `editor/private/editor_panels.cpp`
- `AtomRegistry::registerBuiltinAtoms()` 实现 → `editor/private/editor_atom_registry.cpp`
- `ImGuiLib` 移除 `EcsLib` PUBLIC_DEPS，最终依赖：`imgui::imgui, glad::glad, glfw, WindowLib, LogLib, CoreLib`
- `main.cpp.in` 新增 `#include "editor/editor_panels.h"`，`buildECSInspector` 调用保持不变（现在由 EditorLib 提供）
- ImGuiLib 验证：零 `ecs/` include

---
<!-- 
已完成的 RenderLib 拆分（2026-08-04）：
- RenderLib → RenderCoreLib (rhi/material/phase, zero ECS includes) + RenderSystemLib (render_world/extract/culling/queue/components)
- REFLECT_COMPONENT 统一到 component_registration.cpp
- 测试拆分为 RenderCoreTests + RenderSystemTests
-->

---

## 2. NamePool 迁移为 ECS Resource [中]

**现状**：`StringInternPool`（原 `NamePool`）仍是全局单例 `instance()`（`string_intern_pool.h:27-42`），未接入 ECS Resource 体系。ECS 已有 `ArchetypeWorld` 和 `registerResource` 基础设施可以承载。

**建议**：
- 创建 `NamePoolResource` 结构体，包装 `StringInternPool`
- 在 `World::init()` 或 `initEcs()` 中调用 `registerResource<NamePoolResource>()`
- 迁移现有消费者（如 `scene_serializer.cpp`）从 `NamePool::instance().intern(...)` 改为通过 `world.resource<NamePoolResource>()` 获取

**注意**：`Name` 的编译期构造（`"foo"_name`）不依赖 Intern 池，只有运行期 `intern()` 需要迁移。

---

## 3. 增加 `entelechy_snprintf` 跨平台封装 [低]

**现状**：`string_format.h` 中大量 `toStringBuf` 特化直接调用原生 `snprintf`，无平台差异处理。

**建议**：
- 新增 `core/public/string/platform_snprintf.h` + `core/private/string/platform_snprintf.cpp`
- MSVC 用 `_vsnprintf_s(buffer, buf_size, _TRUNCATE, ...)`，POSIX 用 `vsnprintf`
- 统一截断行为：始终保证 `buffer[buf_size - 1] = '\0'`（`buf_size > 0`）
- 替换 `string_format.h` 中所有裸 `snprintf` 调用

---

## 4. `FunctionRef` — 替代 `std::function` 堆分配 [低]

**现状**：`ALLOCATOR_GUIDE.md` 已文档化约束（lambda 捕获 ≤ 16 字节），但无实际 `FunctionRef` 类型。ThreadPool 的 `std::function` 仍可能在捕获大于 SSO 阈值时触发标准库内部的堆分配，绕过引擎分配器。

**建议**：
- 新增 `core/public/functional/function_ref.h`
- 非拥有型函数引用，类似 C++23 `std::function_ref`，零堆分配
- 替代 ThreadPool 中所有 `std::function<void()>` 为 `FunctionRef<void()>`
- 约束：`FunctionRef` 不拥有被引用对象，调用者需保证生命周期

---

## 历史归档

以下事项已在审计中被确认落实或不适用，从本文档移除：

| 原计划 | 事项 | 处理 |
|--------|------|------|
| 字符串审计 P0 | `append()` union 破坏 bug | ⚪ String 已重写为 `std::basic_string` wrapper |
| 字符串审计 P0 | `operator=(const char*)` UAF | ⚪ 同上，不自赋值风险 |
| 字符串审计 P1 | 扩展 String 单元测试 | ✅ 31 tests in `test_string.cpp` |
| 字符串审计 P3 | `StringView` 类型 | ✅ `string_view.h`，超出计划规格 |
| 字符串审计 P3 | 移动语义源对象清空 | ⚪ `std::basic_string` delegate 保证 |
| 模块依赖 #1 | TestFrameworkLib 排除 | ✅ `CMakeLists.txt:166-170` |
| 模块依赖 #2 | initCore→initEcs | ✅ 已改名 |
| 模块依赖 #5 | CMake FOLDER 属性 | ✅ VS 分层显示 |
| 模块依赖 #4 | BridgeLib 职责 | ◐ `AGENTS.md` + `initBridge` 已加 |
| 分配器 1.1-1.3 | Mimalloc + aliases + guide | ✅ 全部完成 |
| 分配器 2.1-2.4 | ECS 分配器注入 | ✅ 全部完成 |
| 分配器 3.1 | ThreadPool deque 替换 | ✅ `OverflowQueue` |
| 分配器 3.3 | GPUResource 延迟删除 | ✅ fence 机制 |
| 分配器 3.4 | RenderWorld::clear() | ✅ `clearAllEntities` |
| 分配器 4.1 | ArchetypeWorld | ✅ 完整 API |
| 分配器 A.2 | QuantizeSize | ✅ `iallocator.h:31` |
| 分配器 A.4 | IAllocator::realloc | ✅ 2026-08-04 已补充 |
| 分配器 A.5 | Log unique_ptr | ✅ 2026-08-04 已替换为 raw pointer |
| 分配器 A.6 | FrameArenaRing | ✅ `RenderWorld.h:50` |
| 分配器 A.8 | MimallocActive 测试 | ✅ `test_memory.cpp:316` |
