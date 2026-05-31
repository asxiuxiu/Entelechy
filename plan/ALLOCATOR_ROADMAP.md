# Entelechy 分配器与 ECS 存储改进路线图

> **状态**：2026-05-31 由 `self-engine-audit` 产出
> **关联笔记**：`Notes/SelfGameEngine/审计/2026-05-31-分配器与ECS存储改进计划.md`

---

## 执行摘要

项目已建立完整的分配器基础设施（`IAllocator`、`FrameArena`、`ObjectPool`、`DebugAllocatorWrapper`），但**业务代码没有利用这些武器**——ECS 核心层全部硬编码 `DefaultAllocator`（CRT 堆），未接入 Mimalloc，存在 ThreadPool `std::deque` 绕过等漏洞。

本路线图分四个阶段，从基础设施升级到 ECS 存储模型演进，预计总工期 2-3 周。

---

## 阶段一：基础设施升级（1-2 天）

### 1.1 接入 Mimalloc

**原因**：UE 5.5+ 默认分配器已切换为 Mimalloc。P99 延迟比 jemalloc 低 15%。单文件 `static.c` 嵌入，集成成本极低。

**改动文件**：
- `core/allocator/allocator.h`
- 根 `CMakeLists.txt`

```cpp
// allocator.h
struct DefaultAllocator {
    static void* alloc(usize size, usize align = alignof(std::max_align_t)) {
#if ENTELECHY_USE_MIMALLOC
        return mi_malloc_aligned(size, align);
#else
    #if PLATFORM_WINDOWS
        return _aligned_malloc(size, align);
    #else
        if (align <= alignof(std::max_align_t)) return std::malloc(size);
        void* ptr = nullptr;
        posix_memalign(&ptr, align, size);
        return ptr;
    #endif
#endif
    }

    static void free(void* ptr) {
#if ENTELECHY_USE_MIMALLOC
        mi_free(ptr);
#else
    #if PLATFORM_WINDOWS
        _aligned_free(ptr);
    #else
        std::free(ptr);
    #endif
#endif
    }
};
```

```cmake
# CMakeLists.txt
option(ENTELECHY_USE_MIMALLOC "Use mimalloc as global allocator" ON)
if(ENTELECHY_USE_MIMALLOC)
    # 方式 A：git submodule 引入 mimalloc
    # add_subdirectory(third_party/mimalloc)
    # target_link_libraries(EntelechyCore PUBLIC mimalloc-static)
    
    # 方式 B：Conan 包管理（如果项目已用 Conan）
    # find_package(mimalloc REQUIRED)
    # target_link_libraries(EntelechyCore PUBLIC mimalloc::mimalloc)
    
    target_compile_definitions(EntelechyCore PUBLIC ENTELECHY_USE_MIMALLOC=1)
endif()
```

**验收**：运行 `test_memory` 单元测试通过；`GetGlobalAllocatorStats()` 统计正常。

### 1.2 建立分配器别名体系

**新文件**：`core/allocator/allocator_aliases.h`

```cpp
#pragma once
#include "core/allocator/allocator.h"

namespace Entelechy {

// 通用持久对象：走全局分配器（Mimalloc）
using HeapAllocator = DefaultAllocator;

// ECS 组件存储列（Column<T> 的数据缓冲区）
// 当前 fallback 到 DefaultAllocator，未来可切换为 Chunk 分配器
using ColumnAllocator = DefaultAllocator;

// ECS 组件数组对象本身（ComponentArray<T> 的壳）
// 数量极少（= 组件类型数），Phase 2.2 替换为 ObjectPool
using ComponentArrayAllocator = DefaultAllocator;

} // namespace Entelechy
```

### 1.3 撰写 ALLOCATOR_GUIDE.md

**新文件**：`core/allocator/ALLOCATOR_GUIDE.md`（即本文件同目录下的工程文档）

内容见 Vault 笔记。核心是三问题决策清单 + 禁止清单。

---

## 阶段二：ECS 核心层分配器注入（2-3 天）

### 2.1 World 注入 IAllocator

**改动文件**：`ecs/public/ecs/world/world.h`、`ecs/private/world/world.cpp`

```cpp
// world.h
class World {
public:
    explicit World(IAllocator* allocator = GetGlobalAllocator());
    explicit World(EntityRegistry& registry, IAllocator* allocator = GetGlobalAllocator());
    ~World();

private:
    IAllocator* m_allocator;
    // ...
};
```

```cpp
// world.cpp
World::World(IAllocator* allocator)
    : m_allocator(allocator)
    , m_owns_registry(true) {
    void* mem = m_allocator->allocate(sizeof(EntityRegistry), alignof(EntityRegistry));
    m_registry = new (mem) EntityRegistry();
}

World::~World() {
    for (auto pair : m_component_arrays) {
        pair.second->~IComponentArray();
        m_allocator->free(pair.second);
    }
    if (m_owns_registry) {
        m_registry->~EntityRegistry();
        m_allocator->free(m_registry);
    }
}
```

**关键变化**：
- `alloc` + `construct_at` → `allocate` + placement new（C++ 自定义分配器标准惯用法）
- `destroy_at` + `free` → 显式析构 + `free`（保留，因为虚析构需要）

### 2.2 ComponentArray 分配策略分层

**改动文件**：`ecs/public/ecs/component/component_array.h`

Column<T> 注入分配器：

```cpp
template <typename T>
class Column {
public:
    explicit Column(IAllocator* allocator = GetGlobalAllocator())
        : m_allocator(allocator), m_data(nullptr), m_count(0), m_capacity(0) {}

private:
    IAllocator* m_allocator;
    T* m_data;
    usize m_count;
    usize m_capacity;

    void grow() {
        usize newCap = m_capacity == 0 ? 4 : m_capacity * 2;
        // TODO(Phase 1.2): 接入 QuantizeSize
        // usize alignedBytes = QuantizeSize(newCap * sizeof(T));
        // newCap = alignedBytes / sizeof(T);
        
        constexpr usize ALIGN = (alignof(T) > 16) ? alignof(T) : 16;
        T* newData = static_cast<T*>(m_allocator->allocate(newCap * sizeof(T), ALIGN));
        // ...
    }
};
```

World 中 ComponentArray 对象用 ObjectPool（可选，Phase 2.2）：

```cpp
// World 内部增加 ObjectPool 用于 ComponentArray 壳对象
ObjectPool<IComponentArray> m_array_pool{4}; // 初始 4 个 block

// getOrCreateComponentArray 中使用
void* mem = m_array_pool.allocate(sizeof(ComponentArray<T>), alignof(ComponentArray<T>));
auto* array = new (mem) ComponentArray<T>();
```

### 2.3 SparseSet 注入 IAllocator

**改动文件**：`ecs/public/ecs/type/sparse_set.h`、`ecs/private/type/sparse_set.cpp`

```cpp
class SparseSet {
public:
    explicit SparseSet(IAllocator* allocator = GetGlobalAllocator());
    // ...
private:
    IAllocator* m_allocator;
    DynamicArray<u32*> m_pages;
    DynamicArray<u32> m_dense;
};
```

### 2.4 CommandBuffer 改用 FrameArena

**改动文件**：`ecs/public/ecs/world/command_buffer.h`

```cpp
class CommandBuffer {
public:
    explicit CommandBuffer(IAllocator* commandAllocator = GetGlobalAllocator())
        : m_arena(64 * 1024) {} // 64 KiB 命令缓冲区
    
    template<typename T>
    void set(Entity e, const T& value) {
        void* mem = m_arena.allocate(sizeof(SetCommand<T>), alignof(SetCommand<T>));
        m_commands.pushBack(new (mem) SetCommand<T>(e, value));
    }
    
    void apply(World& world) {
        for (auto* cmd : m_commands) {
            cmd->apply(world);
            cmd->~ICommand();
        }
        m_commands.clear();
        m_arena.reset(); // O(1) 回收
    }
    
    void clear() {
        for (auto* cmd : m_commands) {
            cmd->~ICommand();
        }
        m_commands.clear();
        m_arena.reset();
    }

private:
    FrameArena m_arena;
    DynamicArray<ICommand*> m_commands;
    DynamicArray<BatchRange> m_batches;
};
```

**注意**：CommandBuffer 目前由 Scheduler 持有（栈上对象）。如果 CommandBuffer 内部有 FrameArena，Scheduler 的栈内存占用会增加 64 KiB。这在可接受范围内（现代栈通常为 1-8 MiB）。

---

## 阶段三：子系统漏洞修复（1-2 天）

### 3.1 ThreadPool std::deque 替换

**改动文件**：`thread_pool/public/thread_pool/thread_pool.h`

```cpp
// 替换前
std::mutex m_overflow_mutex;
std::deque<std::function<void()>> m_overflow_tasks;

// 替换后：基于 DynamicArray 的简单队列
class OverflowQueue {
public:
    void push(std::function<void()> task);
    bool pop(std::function<void()>& out);
    bool empty() const;
    
private:
    DynamicArray<std::function<void()>> m_tasks;
    usize m_head = 0;
};

// ThreadPool 中
OverflowQueue m_overflow_tasks;
```

### 3.2 std::function 隐形分配约束

**短期方案**（最小改动）：

在 `ALLOCATOR_GUIDE.md` 中增加约束：

```markdown
## ThreadPool 任务约束

提交到 `ThreadPool::submit()` 的 lambda 捕获总量不得超过 16 字节。
超过此限制时，`std::function` 会触发标准库内部的堆分配，绕过引擎分配器体系。

如果需要传递大量上下文，使用显式上下文指针：

```cpp
struct Context { int a; float b; Mesh* mesh; };
Context ctx = { ... };
threadPool.submit([](void* userData) {
    Context* c = static_cast<Context*>(userData);
    // use c->mesh...
}, &ctx);
```
```

**长期方案**：引入 `FunctionRef`（零分配函数引用），见 Vault 笔记。

### 3.3 GPUResource 延迟删除队列

**改动文件**：`render/public/render/rhi/rhi_resources.h`、`render/private/rhi/gl_rhi_device.cpp`

```cpp
class GLRHIDevice {
public:
    void deleteResource(GPUResource* resource);
    void flushPendingDeletes(); // 每帧渲染线程安全点调用
    
private:
    DynamicArray<GPUResource*> m_pending_deletes;
};
```

### 3.4 RenderWorld::clear() 优化

**改动文件**：`render/private/render_world/RenderWorld.cpp`

```cpp
// 当前 O(maxEntityID)
// for (u32 id = 0; id < maxId; ++id) { destroyImmediate(e); }

// 改进：批量重置
void World::clearAllEntities() {
    // 直接重置 registry，避免逐个 destroyImmediate
    m_registry->reset(); // 需要 EntityRegistry 提供 reset 方法
    
    // 批量清理所有 ComponentArray
    for (auto pair : m_component_arrays) {
        pair.second->clear(); // 需要 IComponentArray 提供 clear 方法
    }
    
    m_entity_masks.resize(0);
    m_entity_ranks.resize(0);
}
```

---

## 阶段四：ECS 存储模型演进（1-2 周）

### 4.1 目标

从 SparseSet + Column（SoA 的前身）走向 Archetype Chunk（工业级 ECS 存储）。

### 4.2 设计草图

```cpp
// ecs/public/ecs/archetype/archetype_world.h

using ArchetypeID = u64; // 组件类型位掩码

struct Chunk {
    static constexpr usize SIZE = 16 * 1024; // 16 KiB
    ArchetypeID archetype;
    u8 data[SIZE]; // SoA 布局的原始字节
    u16 count;
    u16 capacity;
};

struct Archetype {
    ArchetypeID id;
    DynamicArray<ComponentTypeID> componentTypes;
    DynamicArray<usize> componentOffsets; // SoA 列偏移
    usize entitySize;
    DynamicArray<Chunk*> chunks;
};

class ArchetypeWorld {
public:
    Entity spawn(ArchetypeID archetype);
    void destroy(Entity e);
    
    template<typename... Cs>
    auto query();

private:
    HashMap<ArchetypeID, Archetype*> m_archetypes;
    // Entity → (Archetype*, chunkIndex, slotIndex)
    // 可用分页数组实现
};
```

### 4.3 迁移路径

**步骤 1**：在现有 `World` 旁新建 `ArchetypeWorld`，不破坏现有代码。
**步骤 2**：选择一个热数据组件（如 `Transform`）在 `ArchetypeWorld` 中存储和查询，验证正确性。
**步骤 3**：逐步将 System 从 `World` 迁移到 `ArchetypeWorld`。
**步骤 4**：实现双轨存储（热数据 Archetype / 冷数据 SparseSet）。
**步骤 5**：完全迁移后，移除旧的 `World` + `ComponentArray` 路径。

### 4.4 决策信号

- 实体数 > 50,000
- Profiler 显示 System 迭代 cache miss > 30%
- 多组件查询（3+ 组件）成为帧时间瓶颈

---

## 验收标准

### 阶段一
- [ ] `DefaultAllocator` 底层可在 CMake 中切换 Mimalloc / CRT
- [ ] `ALLOCATOR_GUIDE.md` 存在
- [ ] `allocator_aliases.h` 存在并被引用

### 阶段二
- [ ] `World` / `Column<T>` / `SparseSet` 构造函数接收 `IAllocator*`
- [ ] `CommandBuffer` 内部使用 `FrameArena`
- [ ] 无 `DefaultAllocator::alloc` + `construct_at` 组合残留

### 阶段三
- [ ] ThreadPool 无 `std::deque`
- [ ] `GPUResource` 延迟删除队列落地
- [ ] `RenderWorld::clear()` 非 O(maxEntityID)

### 阶段四
- [ ] `ArchetypeWorld` 类存在并可创建/销毁实体
- [ ] 至少一个组件类型可在 Archetype 中存储和查询
- [ ] Profiler cache miss 降低 >30%

---

## 附录：遗漏项补充（审计后追加）

以下项在初版路线图中未独立列出，但审计扫描中发现需要处理：

### A.1 `allocator_aliases.h` 文件创建
路线图 1.2 提到了这个文件，但未实际创建。需要在 `core/allocator/` 下建立：
```cpp
#pragma once
#include "core/allocator/allocator.h"
namespace Entelechy {
using HeapAllocator = DefaultAllocator;
using ColumnAllocator = DefaultAllocator;
using ComponentArrayAllocator = DefaultAllocator;
} // namespace Entelechy
```

### A.2 `QuantizeSize` 接口（中优先级）
笔记中明确说："`QuantizeSize` 是容易忽略但收益很高的优化：容器在扩容时调用 `allocator.QuantizeSize(requestedSize)`，而不是直接 `requestedSize`，能显著减少内部碎片。"

**实现**：给 `IAllocator` 或 `DefaultAllocator` 增加 `static usize quantizeSize(usize size)`，在 `DynamicArray::grow()` 和 `Column<T>::grow()` 中调用。

### A.3 全局 `new`/`delete` 重载（低优先级，可选）
UE 的核心机制：全局 `operator new`/`delete` 被重载为走 `FMemory`。这能从根本上防止任何人绕过分配器体系。

**代价**：重载全局 `new` 会影响所有第三方库（如 glad、glfw、imgui）的内存分配。如果第三方库有特殊的分配需求（如 GPU 驱动相关的对齐要求），全局重载可能导致问题。
**建议**：暂时不重载全局 `new`，但保留为可选编译开关（`ENTELECHY_OVERRIDE_GLOBAL_NEW`）。

### A.4 `IAllocator` 缺少 `realloc`（低优先级）
当前 `DynamicArray::grow()` 对 trivially_copyable 类型用 `memcpy`，但对非 trivial 类型需要逐元素 move+destroy。如果 `IAllocator` 有 `realloc`，Mimalloc 的 `mi_realloc_aligned` 可以在原地扩容（当后面有空闲块时），避免数据拷贝。

### A.5 Log 模块 `std::unique_ptr`（低优先级）
`logger.h` 使用 `DynamicArray<std::unique_ptr<LogOutputDevice>> m_devices`。`std::unique_ptr` 的内存分配走标准库默认分配器（`std::default_delete` 不分配，但 `std::make_unique` 用全局 `new`）。
**改进**：用 `DynamicArray<LogOutputDevice*>` + placement new / 显式 delete，或接入引擎分配器。

### A.6 RenderWorld::extract() 每帧构造 FrameArena（低优先级）
当前 `RenderWorld::extract()` 每帧创建局部 `FrameArena arena(1024 * 1024)`。FrameArena 构造函数需要 `std::malloc` 分配 1 MiB 基址内存。每帧都 `malloc` + `free` 是浪费。
**改进**：用 `FrameArenaRing<2>` 作为成员变量，双缓冲复用基址内存。

### A.7 SmallString 高频临时分配（Info 级）
`bridge/agent_bridge.cpp` 和 `scene_serializer.cpp` 中存在大量 `SmallString` 拼接。当 JSON 字段名/路径超过 SSO 阈值（~15-23 字符）时触发堆分配。
**短期**：接受，因为 SmallString 的 SSO 已覆盖大部分场景。
**长期**：高频序列化路径使用预分配的 `StringBuilder` 或 `FixedString<N>`。

### A.8 Mimalloc 接入验证测试
路线图 1.1 缺少具体的验证测试代码。在 `core/tests/test_memory.cpp` 中增加：
```cpp
TEST(Memory, MimallocActive) {
#if ENTELECHY_USE_MIMALLOC
    void* p = mi_malloc(1024);
    EXPECT_NE(p, nullptr);
    mi_free(p);
#endif
}
```

---

## 参考

- Vault 审计笔记：`Notes/SelfGameEngine/审计/2026-05-31-分配器与ECS存储改进计划.md`
- 费曼笔记：`Notes/SelfGameEngine/基础工具层/内存分配器.md`、`Notes/SelfGameEngine/基础工具层/容器系统.md`、`Notes/SelfGameEngine/核心运行时闭环/组件系统架构.md`
