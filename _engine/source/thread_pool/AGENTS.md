# ThreadPool 模块

> 路径：`_engine/source/thread_pool`

## 一句话职责

基于 Chase-Lev 工作窃取算法的线程池：支持任务提交、批量并行 for、全局提交队列 + 本地窃取队列双层结构。

## 关键文件
| 文件 | 用途 |
|------|------|
| `thread_pool.h` | `ThreadPool` 类 + `WorkStealingQueue` 类；任务提交、`parallelFor` 模板、工作窃取逻辑 |
| `thread_pool.cpp` | 线程池主循环、队列 push/pop/steal 实现、全局提交队列处理 |
| `CMakeLists.txt` | `ThreadPoolLib` 静态库定义，依赖 `CoreLib` |

## 重要入口
- 改**线程池生命周期或工作线程数量** → 动 `thread_pool.h/.cpp`（`ThreadPool` 构造/析构）
- 改**工作窃取队列容量或算法** → 动 `thread_pool.h/.cpp`（`WorkStealingQueue`）
- 改**任务提交策略** → 动 `thread_pool.cpp`（`submit()`；当前外部提交一律进全局队列，理由见 submit 注释）
- 改**parallelFor 批处理逻辑** → 动 `thread_pool.h`（`parallelFor` 模板）
- 改**等待/同步策略** → 动 `thread_pool.cpp`（`waitForAll()`）

## 依赖关系
- 向上依赖：
  - [Core 模块](../core/AGENTS.md)（`DynamicArray`、`usize`、`foundation_types.h`）
- 被依赖：
  - 未来可能被 TaskSystem、JobSystem 等上层调度模块依赖

## 架构决策
- `WorkStealingQueue` 采用**固定容量**（默认 4096，向上取整到 2 的幂），避免 `grow()` 与 `steal()` 之间的 use-after-free 竞态
- **`push`/`pop` 仅允许 owner 线程调用**——外部 `submit()` 若直推 worker 本地队列会与 owner 的 `pop()` 在 `m_bottom` 上竞争（2026-08-08 实测：任务丢失 + `bad_function_call` 崩溃），故外部提交一律进全局 mutex 队列（`m_overflow_tasks`）；本地队列保留给未来的 worker 本地任务派生（届时需 MPSC 安全的外部 push 或 TLS owner 路由）
- 构造函数两阶段：先分配全部 Worker 再统一启动线程（worker steal 循环遍历 `m_workers`，边建边启动会撞上 `pushBack` realloc 的 use-after-free）
- `parallelFor` 采用动态索引分发：任务内部通过 `fetch_add` 获取下一批索引，减少尾部等待
- `ThreadPool` 析构时向所有线程发停止信号并 `join`，不强制终止正在运行的任务
- 使用 C++11 原子操作与内存序（`memory_order_acquire`/`release`/`seq_cst`），无外部并发库依赖

## 技术债务

> 统一维护于 [TODO.md](../../../../TODO.md)。本模块相关条目包括：ThreadPool/QueueCapacity。

## 测试
- 模块测试位于 `tests/` 目录（`test_thread_pool.cpp`）
- 测试库名为 `ThreadPoolTests`（OBJECT 库），由 [TestRunner](../test_runner/AGENTS.md) 自动收集链接
- 测试覆盖：1000 任务提交计数、`parallelFor` 10000 元素计算正确性
