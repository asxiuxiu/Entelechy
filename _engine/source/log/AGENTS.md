# Log 模块

> 路径：`_engine/source/log`

## 一句话职责
多通道异步日志系统：控制台、文本文件、JSON Lines 文件输出，支持日志级别过滤、文件滚动，以及按会话独立命名日志文件（含毫秒级时间戳）。

## 关键文件
| 文件 | 职责 |
|------|------|
| `logger.h/.cpp` | `Logger` 单例主控；多设备管理；异步队列调度与刷新 |
| `log_macros.h` | `LOG_INFO` / `LOG_DEBUG` / `LOG_ERROR` 等宏（含文件/函数/行号信息） |
| `log_level.h` | 日志级别枚举（Debug/Info/Warning/Error/Fatal） |
| `log_category.h` | 日志分类枚举（Engine/Window/Input/Render 等） |
| `log_entry.h` | 单条日志数据结构 |
| `queued_log_entry.h` | 异步队列中的日志条目封装 |
| `log_output_device.h` | `LogOutputDevice` 抽象接口（write/flush） |
| `console_output.h/.cpp` | `ConsoleOutput`：stdout 输出，带级别前缀 |
| `file_output.h/.cpp` | `FileOutput`：文本文件输出，支持按大小滚动（rolling） |
| `json_file_output.h/.cpp` | `JsonFileOutput`：JSON Lines (.jsonl) 结构化输出，支持滚动 |
| `log_utils.h` | 日志工具函数（级别转字符串、文件名提取等） |

## 重要入口
- 改**日志宏行为**（如添加/删除字段） → 动 `log_macros.h`
- 改**日志级别/分类定义** → 动 `log_level.h` / `log_category.h`
- 改**文本日志格式** → 动 `file_output.cpp` 的 `write()`
- 改**JSON 日志字段或转义逻辑** → 动 `json_file_output.cpp`
- 改**异步刷新策略**（如批量刷新的阈值） → 动 `logger.cpp`
- 新增**日志输出目标**（如网络、远程） → 新增继承 `LogOutputDevice` 的类

## 依赖关系
- 向上依赖：
  - 无（基础层）
- 被依赖：
  - 几乎所有模块（通过宏调用）

## 架构决策 / 临时约束
- 日志系统通过 header-only 风格宏被大量模块包含，改动 `log_macros.h` 会触发大规模重编译
- `Logger` 目前是单线程模型，异步仅指缓冲队列，未来可能引入真正的后台刷写线程
- FileOutput 和 JsonFileOutput 各自独立实现滚动逻辑，未来可提取公共基类
- 每次 `Logger::init()` 会为文件输出生成带 `_YYYYMMDD_HHMMSS_mmm` 后缀的独立日志文件，避免多次启动混写
- 文本与 JSON 时间戳均包含毫秒精度（如 `2026-05-06T18:01:55.464`）
- `LogFileConfig` 与 `JsonFileOutput` 内部使用 `SmallString` 存储路径，避免栈指针悬垂且消除小字符串堆分配
