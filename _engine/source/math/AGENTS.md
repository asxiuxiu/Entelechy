# Math 模块

> 路径：`_engine/source/math`

## 一句话职责
数学库：向量、矩阵、四元数、AABB、随机数、内存对齐工具。

## 关键文件
| 文件 | 职责 |
|------|------|
| `vec.h` | 向量（Vec2 / Vec3 / Vec4）及运算 |
| `mat4.h` | 4×4 矩阵及运算 |
| `quat.h` | 四元数及旋转相关运算 |
| `aabb.h` | 轴对齐包围盒（AABB） |
| `random.h` | 随机数生成器 |
| `align.h` | 内存对齐工具宏/函数 |
| `math_lib.h` | 数学库聚合头文件（引入所有子头文件），版本号 |
| `math_lib.cpp` | 复杂数学运算实现（如 `Mat4::fromRotation`） |

## 重要入口
- 改**向量/矩阵运算** → 动 `vec.h` / `mat4.h`
- 改**旋转/四元数** → 动 `quat.h` / `math_lib.cpp`
- 改**碰撞/包围盒** → 动 `aabb.h`

## 依赖关系
- 向上依赖：
  - 无（纯数学，无外部依赖）
- 被依赖：
  - [Core 模块](../core/AGENTS.md)（World 中可能用到）
  - [System 模块](../system/AGENTS.md)（MovementSystem 等）
  - [Render 模块](../render/AGENTS.md)（投影/视图矩阵）
  - [Runtime 模块](../../_game/source/runtime/AGENTS.md)

## 架构决策 / 临时约束
- 目前是手写数学库，未来可能根据性能测试决定是否替换为 SIMD 或第三方库（如 DirectXMath、glm）
- 头文件尽量保持轻量，避免模板膨胀
