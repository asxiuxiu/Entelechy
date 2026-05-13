# Math 模块

> 路径：`_engine/source/math`

## 一句话职责
数学库：向量、矩阵、四元数、AABB、射线、视锥体、随机数、内存对齐工具、SIMD 抽象层、半精度浮点、ECS Transform 组件与传播系统。

## 关键文件
| 文件 | 职责 |
|------|------|
| `math_config.h` | NaN/Inf 诊断宏开关 (`MATH_ENABLE_NAN_CHECKS`) |
| `vec.h` | 向量（Vec2 / Vec3 / Vec4）；Vec4 为 `alignas(16)` SIMD 运算格式 |
| `mat4.h` | 4×4 列主序矩阵及运算；含 `inverse()`、`determinant()` |
| `quat.h` | 四元数及旋转相关运算；含 `nlerp`（默认）与 `slerp` |
| `aabb.h` | 轴对齐包围盒（AABB），含 `intersects`、`contains` |
| `ray.h` | 射线（Ray），含 `intersectAABB`（Slab method） |
| `frustum.h` | 视锥体（Frustum），含 `fromMatrix`、`intersectsAABB` |
| `random.h` | 确定性随机数生成器 `RandomXORShift32`，含 `nextFloat01` |
| `align.h` | 内存对齐工具宏/函数 |
| `simd.h` | 跨平台 SIMD 抽象层（SSE / NEON / 标量回退），编译期内联 |
| `half.h` | IEEE 754 Float16，用于顶点压缩与 GPU 带宽优化 |
| `transform_component.h` | ECS 组件 `Transform`（本地 TRS + 脏标记）与 `GlobalTransform`（世界矩阵缓存） |
| `transform_propagation_system.h/.cpp` | 引擎内置 System：将脏标记的 `Transform` 烘焙到 `GlobalTransform.matrix` |
| `math_lib.h` | 数学库聚合头文件（引入所有子头文件），版本号 |
| `math_lib.cpp` | 复杂数学运算实现（`Mat4::fromRotation`、`inverse`、`determinant`）及 SIMD 批量测试函数 |

## 重要入口
- 改**向量/矩阵运算** → 动 `vec.h` / `mat4.h`
- 改**旋转/四元数** → 动 `quat.h` / `math_lib.cpp`
- 改**碰撞/包围盒/射线** → 动 `aabb.h` / `ray.h` / `frustum.h`
- 改**SIMD 抽象层** → 动 `simd.h`
- 改**ECS 变换组件** → 动 `transform_component.h`
- 改**Transform 传播行为** → 动 `transform_propagation_system.h/.cpp`

## 依赖关系
- 向上依赖：
  - [Base 模块](../base/AGENTS.md)（`u32`/`usize` 用于随机数/对齐/平台宏）
  - [Core 模块](../core/AGENTS.md)（`World`、`Query`、`Scheduler` 用于 `TransformPropagationSystem`）
- 被依赖：
  - [Core 模块](../core/AGENTS.md)（World 中可能用到）
  - [Render 模块](../render/AGENTS.md)（投影/视图矩阵）
  - [Runtime 模块](../../_game/source/runtime/AGENTS.md)

## 架构决策 / 约束
- **列主序矩阵**：`m[col * 4 + row]`，与 OpenGL / HLSL / glam 保持一致。
- **存储层 vs 运算层分离**：`Vec3` 紧凑 12 字节用于存储；`Vec4` `alignas(16)` 用于 SIMD 运算。
- **SIMD 策略**：编译期多态（`ARCH_X86` / `ARCH_ARM` / 标量回退），无运行时分发，全部可内联。
- **浮点确定性**：`RandomXORShift32` 种子可控；CMake 默认 `/fp:precise`（MSVC），不启用 `-ffast-math`。
- **ECS 集成**：`Transform` / `GlobalTransform` 分离，脏标记避免每帧全量计算；所有数学类型保持纯 POD。
- **System 归属**：`TransformPropagationSystem` 跟随 Transform 数据定义，放在 `math/` 而非集中式 `system/`。
