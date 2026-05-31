# Entelechy 分配器使用指南

> **位置**：`_engine/source/core/allocator/ALLOCATOR_GUIDE.md`
> **关联**：`plan/ALLOCATOR_ROADMAP.md`

---

## 一、核心原则

**容器负责数据结构逻辑，分配器负责内存来源，两者通过最小接口解耦。**

这是 UE `TArray<T, Allocator>` 的核心洞察，也是 Entelechy `DynamicArray<T, AllocatorT>` 的设计基础。不要在业务代码中把"怎么存数据"和"从哪里申请内存"混在一起。

---

## 二、分配器选择决策清单

新增子系统/类/数据结构在申请内存前，必须回答以下三个问题：

### 问题 1：生命周期是否严格等于一帧？

- **是** → 使用 **FrameArena**（从外部注入，不要自己创建）
  - 典型场景：临时渲染命令、System 查询结果缓冲区、每帧的拓扑排序数组
  - 约束：FrameArena 的 `free()` 是 no-op，所有分配的内存会在 `reset()` 时统一回收。不要把跨帧数据（如 Entity 句柄、组件指针）存进 FrameArena。
  - 注入方式：通过函数参数传入 `FrameArena& arena`，不要作为成员变量持有。

- **否** → 继续问题 2

### 问题 2：对象大小是否固定？

- **是** → 使用 **ObjectPool<T>**（动态分块 + 分代索引）
  - 典型场景：粒子、音频通道、网络包、`ComponentArray<T>` 壳对象（数量 = 组件类型数）
  - 约束：对象大小必须编译期确定。不要在 ObjectPool 中存变长数据（如字符串、变长数组）。
  - 句柄安全：总是返回 `PoolHandle` 而非裸指针，避免悬垂访问。

- **否** → 继续问题 3

### 问题 3：分配频率是否极高（每帧 >1000 次）？

- **是** → 审查是否应改为 Arena/Pool 方案。如果必须用通用分配，确保走 **DefaultAllocator**（底层已是 Mimalloc）。
  - 高频小对象分配如果走 CRT 堆，即使 Mimalloc 也会引入不必要的竞争。

- **否** → 使用 **DefaultAllocator**（底层 Mimalloc）
  - 通用持久对象的默认选择。不要试图优化每一个分配点——Mimalloc 已经足够快。

---

## 三、禁止清单（Hard No）

| 禁止项 | 原因 | 替代方案 |
|--------|------|---------|
| ❌ `std::malloc` / `std::free` | 绕过追踪、无法切换 Mimalloc、Debug 构建无诊断 | `DefaultAllocator::alloc/free` |
| ❌ 裸 `new` / `delete`（placement new 除外） | 同上 | placement new + `IAllocator::allocate` |
| ❌ `std::vector` / `std::string` / `std::unordered_map` | 分配器不可控、异常安全冲突、cache 不友好 | `DynamicArray` / `SmallString` / `HashMap` |
| ❌ `std::deque` | 默认 `std::allocator`，绕过引擎分配器体系 | `DynamicArray` 实现的环形缓冲区 |
| ❌ `std::function` 大捕获 | lambda 捕获 >16 字节时内部 `new` 分配，走标准库默认分配器 | `FunctionRef`（零分配）或小捕获 + 上下文指针 |
| ❌ FrameArena 存跨帧指针 | 帧末 `reset()` 后指针悬垂 | 跨帧数据走 `DefaultAllocator` 或 `ObjectPool` |
| ❌ 子系统自己创建 FrameArena | FrameArena 应该是外部注入的，否则子系统无法共享/协调帧内存 | 从 `Scheduler` 或调用方接收 `FrameArena&` |

---

## 四、各子系统默认分配策略

| 子系统 | 持久数据 | 临时数据 | 备注 |
|--------|---------|---------|------|
| **ECS (World)** | `IAllocator*` 注入 → `DefaultAllocator` | `FrameArena`（CommandBuffer 内部） | ComponentArray 壳对象未来切 ObjectPool |
| **ECS (Column)** | `IAllocator*` 注入 → `DefaultAllocator` | — | 数据缓冲区，真正的大内存 |
| **ECS (SparseSet)** | `IAllocator*` 注入 → `DefaultAllocator` | — | 页面分配 |
| **Render (RHI)** | `DefaultAllocator` | 局部 `FrameArena` | GPUResource 走延迟删除队列 |
| **Render (Material)** | `DefaultAllocator` | — | Uniform 数据缓冲区 |
| **ThreadPool** | `DefaultAllocator`（WorkStealingQueue buffer） | — | 溢出队列禁用 `std::deque` |
| **Asset** | `DefaultAllocator` | `FrameArena`（加载时临时缓冲） | 大资源缓存未来可切专用池 |
| **Log** | `DefaultAllocator` | — | 当前 `std::unique_ptr` 走标准堆，未来接入 |
| **VFS** | `DefaultAllocator` | `FrameArena`（读取缓冲） | — |

---

## 五、代码模式参考

### 模式 A：placement new + IAllocator（自定义类对象）

```cpp
// 创建
void* mem = allocator->allocate(sizeof(MyClass), alignof(MyClass));
MyClass* obj = new (mem) MyClass(args...);

// 销毁
obj->~MyClass();
allocator->free(obj);
```

**不要用**：`DefaultAllocator::alloc` + `std::construct_at`——这是反模式重造，placement new 更简洁且是 C++ 标准惯用法。

### 模式 B：容器 + 分配器模板参数

```cpp
// 默认走 DefaultAllocator（Mimalloc）
DynamicArray<Entity> entities;

// 临时数据走 FrameArena（从外部注入）
DynamicArray<DrawCmd, FrameArenaAllocator> frameCmds(arena);

// 固定大小对象走 ObjectPool
ObjectPool<Particle> particlePool;
DynamicArray<Particle*, PoolAllocator<Particle>> particleRefs(pool);
```

### 模式 C：FrameArena 的正确打开方式

```cpp
// ✅ 正确：从外部接收，作用域内使用
void MySystem::tick(World& world, FrameArena& arena, f32 dt) {
    auto& temp = *arena.alloc<DynamicArray<Entity>>(1);
    new (&temp) DynamicArray<Entity>();
    // ... 使用 temp ...
    temp.~DynamicArray<Entity>(); // 如果类型有析构，必须手动调用
}

// ❌ 错误：自己创建 FrameArena
void MySystem::tick(World& world, f32 dt) {
    FrameArena arena(1024 * 1024); // 不要这样做！
    // ...
}

// ❌ 错误：把 FrameArena 分配的数据存为成员变量
class MySystem {
    Entity* m_tempBuffer; // 如果这是 FrameArena 分配的，reset() 后悬垂
};
```

### 模式 D：ObjectPool 的使用

```cpp
ObjectPool<ComponentArrayBase> arrayPool{4}; // 初始 4 个 block

// 分配
PoolHandle h = arrayPool.alloc();
ComponentArrayBase* arr = arrayPool.resolve(h);

// 释放
arrayPool.free(h);

// 安全访问
if (arrayPool.isValid(h)) {
    arr->doSomething();
}
```

---

## 六、常见问题

### Q1：为什么不用 `std::construct_at`，而用 placement new？

两者语义等价，但 placement new 是 C++ 自定义分配器的标准惯用法。UE 的全局 `new` 重载、Bevy 的 Rust `Box::new_in` 都遵循这一模式。`construct_at` 是 C++20 的工具函数，更适合在已经拥有正确类型指针的场景使用（如容器内部的元素构造）。

### Q2：Mimalloc 集成后，怎么确认它真的在工作？

```cpp
// 在 main() 或测试代码中
#include <mimalloc.h>

void checkMimalloc() {
#if ENTELECHY_USE_MIMALLOC
    void* p = mi_malloc(1024);
    CHECK(p != nullptr);
    mi_free(p);
    printf("Mimalloc version: %s\n", mi_version());
#endif
}
```

或者在 `DefaultAllocator::alloc` 中加一个 `CHECK(false && "CRT allocator still in use")` 的 fallback，确保不会意外走到 CRT 路径。

### Q3：FrameArena 的 `free()` 是 no-op，那如果我在 FrameArena 上构造了有析构函数的对象怎么办？

**必须手动析构**。FrameArena 只管理原始字节的分配和回收，不管理对象生命周期。

```cpp
// 错误：内存被 reset() 回收了，但对象没析构，泄漏了非 POD 资源
auto* str = arena.alloc<SmallString>(1);
new (str) SmallString("hello");
// arena.reset(); // ❌ str 的堆内存泄漏了！

// 正确
str->~SmallString();
arena.reset();
```

**结论**：FrameArena 只适合存 POD 或 trivially destructible 的数据。如果必须存非 POD，使用 `MemMark` 作用域 + 手动析构，或者改用 `DefaultAllocator`。

### Q4：`IAllocator` 只有 `allocate`/`free`，没有 `realloc` 怎么办？

当前 `IAllocator` 确实缺少 `realloc`。对于 `DynamicArray::grow()` 这类场景，目前的做法是：
1. `allocate(newSize)` 新内存
2. 逐元素 move-construct
3. 逐元素 destroy
4. `free(oldPtr)`

如果将来需要优化 trivially_copyable 类型的扩容路径，可以考虑给 `IAllocator` 增加 `realloc` 接口，或给 `DefaultAllocator` 增加静态 `realloc` 方法。

---

## 七、审查清单（Code Review 时使用）

提交涉及内存分配的 PR 时，审查者必须检查：

- [ ] 没有裸 `new`/`delete`/`malloc`/`free`
- [ ] 没有 `std::vector`/`std::string`/`std::unordered_map`/`std::map`/`std::deque`
- [ ] `std::function` 的 lambda 捕获量 < 16 字节（或已改用 `FunctionRef`）
- [ ] FrameArena 分配的内存没有跨帧存活
- [ ] 自定义对象的构造/析构成对出现（placement new ↔ 显式析构）
- [ ] 如果新增容器类，是否支持分配器模板参数
