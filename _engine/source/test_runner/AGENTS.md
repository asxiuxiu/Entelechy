# TestRunner 模块

> 路径：`_engine/source/test_runner`

## 一句话职责
统一测试入口可执行文件（`EntelechyTests`）：自动收集所有模块的 OBJECT 测试并运行。

## 关键文件

| 文件 | 用途 |
|------|------|
| `main.cpp` | 极简入口：`return Entelechy::Test::TestRegistry::instance().runAll();` |
| `CMakeLists.txt` | `EntelechyTests` 可执行文件定义；通过 `if(TARGET)` 自动发现各模块 `tests/` 的 OBJECT 库 |

## CMake 自动发现机制

`CMakeLists.txt` 维护一个 `POSSIBLE_TEST_TARGETS` 列表，包含所有模块的测试对象库名（如 `BaseTests`、`VFSTests`、`ThreadPoolTests`）。通过遍历该列表并检查 `if(TARGET ${target})`，自动跳过尚未创建测试的模块，然后将存在的测试通过 `$<TARGET_OBJECTS:${target}>` 链接到最终可执行文件。

这种方式**不依赖模块处理顺序**，`test_runner` 在 `cmake_projects.json` 中的位置不受限制。

## 依赖关系

- 向上依赖：
  - [Test 框架模块](../test/AGENTS.md)（`TestFrameworkLib`）
  - 所有引擎模块库（`CoreLib`、`EcsLib`、`VFSLib`、`ThreadPoolLib` 等）
  - 所有存在 `tests/` 的模块的 OBJECT 测试库
- 被依赖：
  - 无（叶子节点，可执行文件）
