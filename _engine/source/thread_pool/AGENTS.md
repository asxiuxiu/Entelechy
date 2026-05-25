# ThreadPool 模块

> 路径：`_engine/source/thread_pool`

## 一句话职责
基于 Chase-Lev 工作窃取算法的线程池：支持任务提交、批量并行 for、以及本地队列溢出回退。

## 关键文件

| 文件 | 用途 |
|------|------|
| `thread_pool.h` | `ThreadPool` 类 + `WorkStealingQueue` 类；任务提交、`parallelFor` 模板、工作窃取逻辑 |
| `thread_pool.cpp` | 线程池主循环、队列 push/pop/steal 实现、溢出队列处理 |
| `CMakeLists.txt` | `ThreadPoolLib` 静态库定义，依赖 `BaseLib` |

## 重要入口

- 改**线程池生命周期或工作线程数量** → 动 `thread_pool.h/.cpp`（`ThreadPool` 构造/析构）
- 改**工作窃取队列容量或算法** → 动 `thread_pool.h/.cpp`（`WorkStealingQueue`）
- 改**任务提交策略**（轮询分发、溢出行为） → 动 `thread_pool.cpp`（`submit()`）
- 改**parallelFor 批处理逻辑** → 动 `thread_pool.h`（`parallelFor` 模板）
- 改**等待/同步策略** → 动 `thread_pool.cpp`（`waitForAll()`）

## 依赖关系

- 向上依赖：
  - [Base 模块](../base/AGENTS.md)（`DynamicArray`、`usize`、`foundation_types.h`）
- 被依赖：
  - [TestBatch0 模块](../test_batch0/AGENTS.md)（验收测试用例）
  - 未来可能被 TaskSystem、JobSystem 等上层调度模块依赖

## 架构决策 / 临时约束

- `WorkStealingQueue` 采用**固定容量**（默认 4096，向上取整到 2 的幂），避免 `grow()` 与 `steal()` 之间的 use-after-free 竞态
- 本地队列满时任务回退到全局 `overflowMutex` + `std::deque` 溢出队列，非最优但足够安全
- `push` / `pop` 仅允许 owner 线程调用；`steal` 允许其他 worker 线程调用
- `parallelFor` 采用动态索引分发：任务内部通过 `fetch_add` 获取下一批索引，减少尾部等待
- `ThreadPool` 析构时向所有线程发停止信号并 `join`，不强制终止正在运行的任务
- 使用 C++11 原子操作与内存序（`memory_order_acquire`/`release`/`seq_cst`），无外部并发库依赖

## 测试

- 模块测试位于 `tests/` 目录（`test_thread_pool.cpp`）
- 测试库名为 `ThreadPoolTests`（OBJECT 库），由 [TestRunner](../test_runner/AGENTS.md) 自动收集链接
- 测试覆盖：1000 任务提交计数、`parallelFor` 10000 元素计算正确性

