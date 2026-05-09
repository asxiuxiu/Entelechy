# Entelechy 分配器选择指南

> 本指南帮助开发者为特定内存使用场景选择正确的分配器。

## 决策树

```
对象生命周期是否严格等于一帧？
  ├── 是 ──► FrameArena
  │           - 所有临时内存共享一个缓冲区
  │           - reset() 在帧尾 O(1) 回收全部
  │           - 支持嵌套 MemMark 回滚
  │           - 溢出自动回退到堆（reset 时释放）
  │
  └── 否 ──► 对象大小是否固定？
              ├── 是 ──► ObjectPool
              │           - 动态分块（每块 1024 slots）
              │           - 分代句柄 PoolHandle（安全失效检测）
              │           - O(1) alloc / free
              │
              └── 否 ──► 通用小对象
                          - DefaultAllocator / DefaultAllocatorV
                          - Debug 构建自动包裹 DebugAllocatorWrapper
                            （Canary + Poison + 统计）
                          - Release 构建零开销
                          - 后续可无缝切换至 Mimalloc 等第三方分配器
```

## 快速对照表

| 场景 | 推荐分配器 | 说明 |
|------|-----------|------|
| 帧内临时缓冲区（字符串拼接、临时数组） | `FrameArena` | 无单独 free，reset 即回收 |
| ECS 组件数组内部存储 | `Column<T>` | SIMD 对齐，dense 紧凑布局 |
| 长期存活的游戏对象 | `ObjectPool` | 分代句柄，防止 use-after-free |
| 容器内部数组扩容 | `DefaultAllocator` | 通过 `IAllocator*` 或模板参数注入 |
| 需要跨线程传递的数据包 | `RingBuffer` | SPSC 无锁，固定容量 |

## 注意事项

- `FrameArena` 的 `free()` 是空操作；不要对 arena 内存单独 free
- `ObjectPool` 返回 `PoolHandle` 而非裸指针；旧句柄在释放后 `isValid()` 为 `false`
- Debug 构建下所有通过 `GetGlobalAllocator()` 的分配自动带 Canary 保护
- `DynamicArray` / `HashMap` 默认使用 `DefaultAllocator` 静态接口；可通过模板参数替换
