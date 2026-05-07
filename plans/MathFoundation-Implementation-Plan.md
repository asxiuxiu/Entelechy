# 数学基础实施计划（Math Foundation Implementation Plan）

> **前置依赖**：Foundation 模块已就绪（基础类型、平台宏）
> **目标**：把现有手写数学库从「能跑」演进到「与 ECS 天生契合、跨平台 SIMD 就绪、浮点确定性可控」的最小工业级集合
> **参考知识源**：`Notes/SelfGameEngine/基础工具层/数学基础.md`

---

## ✅ 已完成（当前状态）

以下功能已实现并可用，从计划中移除详细 Task，仅作状态记录：

| 阶段 | 内容 | 状态 |
|------|------|------|
| 阶段 0 | Vec2 / Vec3 / Vec4 标量运算（+-*/、点积、叉积、归一化） | ✅ |
| 阶段 0 | Mat4 列主序存储与基础运算（乘法、转置、transformPoint/Vector） | ✅ |
| 阶段 0 | Mat4 常用工厂函数（identity、TRS、perspective、ortho、lookAt） | ✅ |
| 阶段 0 | Quat 基础运算（乘、共轭、逆、fromAxisAngle、fromEuler） | ✅ |
| 阶段 0 | SLERP（球面线性插值） | ✅ |
| 阶段 0 | AABB 基础（fromCenterExtent、fromMinMax、expand） | ✅ |
| 阶段 0 | RandomXORShift32（确定性随机数种子） | ✅ |
| 阶段 0 | 对齐工具（AlignUp、AlignUpPtr、ALIGN_SIMD_16） | ✅ |
| 阶段 0 | MathLib 已注册进构建系统（CMake + Source Forest） | ✅ |

---

## 架构决策摘要

### 1. 存储层 vs 运算层分离
- **存储层**：`Vec3` 保持 12 字节紧凑，作为 ECS 组件和顶点数据的存储格式。
- **运算层**：`Vec4` 作为 16 字节对齐的 SIMD 运算格式，是 `Vec3` 在寄存器中的工作形态。
- 矩阵**统一采用列主序**（`m[col * 4 + row]`），与 OpenGL / HLSL / glam 保持一致，避免 GPU 上传前转置的心智负担。

### 2. SIMD 策略：编译期多态 + 标量回退
- 不运行时检测 CPU 特性，不用函数指针分发。
- 通过平台宏（`ARCH_X86`、`ARCH_ARM`）在编译期选择 SSE/NEON/标量回退，保证极热路径可内联。
- 存储默认 16 字节对齐，加载使用未对齐指令（`_mm_loadu_ps` 等），避免内存膨胀。

### 3. 浮点确定性策略
- 随机数彻底屏蔽 `std::random_device`，使用 `RandomXORShift32`。
- 在需要确定性的模块（物理积分、动画评估）局部关闭 `fast-math`；关键路径显式写成 `(a * b) + c`，避免编译器自动 FMA。
- Development 构建加入可开关的 NaN/Inf 检查断言。

### 4. ECS 集成模式
- 本地变换与世界变换**分离为两个组件**：`Transform`（本地 TRS）和 `GlobalTransform`（缓存的世界 Mat4）。
- 脏标记传播：只有 `Transform` 或父节点变化时才重新计算 `GlobalTransform`。
- 所有数学类型保持纯 POD，无虚函数、无动态分配，可直接平铺到 ECS 连续内存。

---

## 阶段 1：规范与补完 —— 夯实接口与对齐保证

**目标**：修正现有实现与笔记设计之间的缺口，建立稳定的数学类型契约。

**验收标准**：
- [ ] `Vec4` 声明为 `alignas(16)`，且 sizeof(Vec4) == 16。
- [ ] `Mat4::fromRotation` 实现已存在于 `math_lib.cpp` 中（当前已有声明，需确认实现完整）。
- [ ] 新增 `nlerp`（归一化线性插值），作为默认快速旋转插值。
- [ ] 所有数学类型在 Development 构建下支持可选的 NaN/Inf 诊断断言（通过宏开关）。
- [ ] 新增 `Mat4::inverse()` 与 `Mat4::determinant()`（渲染和物理都需要）。

### Task 1.1 Vec4 对齐与内存契约
- 给 `Vec4` 添加 `alignas(16)`。
- 在 `math_lib.h` 或新增 `math_config.h` 中定义 `MATH_ENABLE_NAN_CHECKS` 宏（Development 默认开启，Shipping 关闭）。
- 为 `Vec3`/`Vec4`/`Quat` 的 `normalized()`、`length()` 等可能产生 NaN 的函数添加诊断断言。

### Task 1.2 Quat 插值补完
- 新增 `nlerp(const Quat& a, const Quat& b, float t) -> Quat`。
- 明确语义：默认动画/粒子混合用 NLERP；SLERP 保留给需要恒定角速度的关键路径。

### Task 1.3 Mat4 逆矩阵与行列式
- 在 `mat4.h` 中声明 `inverse()` 与 `determinant()`。
- 在 `math_lib.cpp` 中实现（基于伴随矩阵法，4×4 足够小，直接展开）。

### Task 1.4 接口一致性清理
- 统一工厂函数命名：现有 `fromTranslation`、`fromScale`、`fromRotation` 等，检查是否与笔记推荐的 `Translate`、`Scale` 风格冲突。保持现有命名或统一为 `fromXxx`（已在用），只需确保文档一致。
- 给所有 `[[nodiscard]]` 函数补全标记（当前大部分已有，检查遗漏）。

---

## 阶段 2：SIMD 抽象层 —— 跨平台批量运算

**目标**：建立平台无关的 SIMD 命名空间，为后续 SoA 批量矩阵更新提供基础。

**验收标准**：
- [ ] `simd::Vec4F`、`simd::Add`、`simd::Mul`、`simd::Fma` 等基础运算在 x86/SSE 下编译通过。
- [ ] 同一份上层代码在标量回退路径下也编译通过（用于无 SIMD 的平台或调试）。
- [ ] 提供 `simd::Load4F(const float*)` 和 `simd::Store4F(float*, Vec4F)`，使用未对齐加载指令。

### Task 2.1 创建 `simd.h` / `simd.cpp`
- 文件位置：`_engine/source/math/simd.h`
- 结构：
  ```cpp
  namespace Entelechy::simd {
  #if defined(ARCH_X86)
      using Vec4F = __m128;
      inline Vec4F Load4F(const float* p) { return _mm_loadu_ps(p); }
      inline Vec4F Add(Vec4F a, Vec4F b) { return _mm_add_ps(a, b); }
      // ... Mul, Sub, Div, Fma (若可用)
  #elif defined(ARCH_ARM)
      using Vec4F = float32x4_t;
      // ... NEON 路径
  #else
      struct Vec4F { float v[4]; };
      // ... 标量回退，但保持 16 字节对齐
  #endif
  } // namespace Entelechy::simd
  ```
- 标量回退时，`Vec4F` 仍需 `alignas(16)`，确保内存布局跨架构一致。

### Task 2.2 平台宏定义
- 在 Foundation 模块（或 math 模块）定义 `ARCH_X86`、`ARCH_ARM`、`ARCH_WASM` 等宏，基于编译器预定义宏（`_M_X64`、`__aarch64__`、`__wasm__` 等）。

### Task 2.3 最小批量运算示例
- 在 `math_lib.cpp` 中实现一个内部测试函数 `BatchVec4Add(const float* a, const float* b, float* out, usize count)`，用 SIMD 路径和标量路径分别实现，用于验证抽象层正确性。
- 编写一个最小单元测试（可放在 `tests/` 或临时 `main()` 中），验证 SSE 与标量回退结果在 1e-6 精度内一致。

---

## 阶段 3：确定性与辅助类型 —— 让网络同步和 GPU 压缩有根基

**目标**：补齐 `RandomXORShift32` 的浮点输出、引入 `Float16`、建立编译级确定性策略。

**验收标准**：
- [ ] `RandomXORShift32::nextFloat01()` 实现并验证：相同种子在不同平台产生相同序列。
- [ ] `Float16` 类型存在，支持 `FromFloat32` / `ToFloat32`，查表法转换误差 < 0.1%。
- [ ] MathLib 的 CMakeLists.txt 在 Debug 配置下不开启 `/fp:fast`（MSVC）或 `-ffast-math`（Clang/GCC）。

### Task 3.1 RandomXORShift32 浮点扩展
- 按笔记实现 `nextFloat01()`（`0x3F800000` 位技巧）。
- 可选：新增 `nextFloatRange(float min, float max)`。

### Task 3.2 Float16 查表转换
- 新建 `half.h`：
  ```cpp
  struct Float16 { u16 bits; };
  Float16 Float16FromFloat32(f32 value);
  f32 Float32FromFloat16(Float16 h);
  ```
- 实现策略：参考笔记的 base_table + shift_table（float→half）和 mantissa_table + exponent_table（half→float）。
- 单元测试：随机 float 值 round-trip 后误差在可接受范围。

### Task 3.3 编译器浮点确定性开关
- 在 `MathLib` 的 `CMakeLists.txt` 中：
  - MSVC：`/fp:precise`（默认即可，显式禁止 `/fp:fast`）。
  - Clang/GCC：不添加 `-ffast-math`。
- 添加选项 `ENTELECHY_MATH_STRICT_FP`，开启后在关键路径禁用 FMA（MSVC `/fp:strict`，GCC `-ffp-contract=off`）。

---

## 阶段 4：ECS 集成 —— Transform / GlobalTransform 与脏标记

**目标**：把数学库与 ECS 数据层打通，建立层次化世界变换系统。

**验收标准**：
- [ ] `core` 或 `system` 模块中定义 `Transform` 和 `GlobalTransform` 组件。
- [ ] `Transform` 包含本地 `translation: Vec3`、`rotation: Quat`、`scale: Vec3`。
- [ ] `GlobalTransform` 包含 `matrix: Mat4`。
- [ ] 脏标记机制：修改 `Transform` 后，层级传播系统能按需更新 `GlobalTransform`。
- [ ] 无父节点的实体，`GlobalTransform` 等于 `Transform` 的本地矩阵。

### Task 4.1 组件定义
- 确定归属：若 `core` 模块负责组件定义，则在 `_engine/source/core/` 下新增 `transform_component.h`；若决定放在 `math` 模块，则新增 `_engine/source/math/transform.h`。
- 按笔记分离：
  ```cpp
  struct Transform {
      Vec3 translation;
      Quat rotation;
      Vec3 scale;
      u32 dirty : 1;
  };
  struct GlobalTransform {
      Mat4 matrix;
  };
  ```

### Task 4.2 脏标记传播系统（最小实现）
- 在 `system` 模块新增 `TransformPropagationSystem`：
  - 遍历所有 `dirty == true` 的 `Transform`。
  - 自顶向下（或按层级深度排序后）计算世界矩阵：`parent_global * local_trs`。
  - 将结果写入 `GlobalTransform.matrix`，并清除 dirty 标记。
- 若 ECS 暂无父子关系存储，先用「无层级」简化版：`GlobalTransform = TRS(Transform)`，待场景图模块加入后再扩展父子传播。

### Task 4.3 验证
- 在现有 demo 中创建一个带 `Transform` 的实体，Inspector 修改 translation 后，屏幕上的物体位置正确更新。

---

## 阶段 5：几何查询扩展 —— Ray、视锥与批量相交

**目标**：补齐渲染器需要的视锥剔除和鼠标拾取基础工具。

**验收标准**：
- [ ] `Ray` 结构体存在（origin + direction）。
- [ ] `Ray::intersectAABB` 实现（Slab method），返回相交距离 t。
- [ ] `Frustum` 结构体存在（6 个平面），支持 `Frustum::intersectsAABB`。

### Task 5.1 Ray 与 AABB 相交
- 新建 `ray.h`：
  ```cpp
  struct Ray {
      Vec3 origin;
      Vec3 direction; // 必须已归一化
      bool intersectAABB(const AABB& box, float& out_t) const;
  };
  ```
- 按笔记实现 Slab method。

### Task 5.2 视锥体（Frustum）
- 新建 `frustum.h`：
  - 从投影矩阵（perspective / ortho）提取 6 个平面。
  - `bool intersectsAABB(const AABB& box) const`：若 AABB 在所有平面的负半空间外，则不相交。
- 这是视锥剔除的基石，保持标量实现即可（分支少，足够快）。

### Task 5.3 AABB 与 AABB 相交补完
- 当前 `aabb.h` 缺少 `intersects(const AABB& other)`，补充实现。

---

## 阶段 6：回探替换与性能验证

**目标**：清理阶段 2 的临时实现，建立性能基线。

**验收标准**：
- [ ] 项目中不再使用 `std::array<float, 3>` 或裸 `float[3]` 表示空间坐标。
- [ ] 不再使用 `rand()` 或 `std::mt19937` 做 gameplay 随机数。
- [ ] 10,000 个实体的 `GlobalTransform` 批量更新在 Debug 配置下稳定 60fps+（当前标量路径即可，SIMD 优化作为后续加分项）。

### Task 6.1 回探替换清单
| 临时代码 | 替换为 | 验证方法 |
|----------|--------|----------|
| `std::array<float, 3>` 或裸 `float[3]` | `Vec3` / `Quat` / `Mat4` | 全局搜索，编译通过 |
| 硬编码 MVP 矩阵组装（如有） | `Mat4::fromTRS` / `lookAt` / `perspective` | 屏幕上立方体位置正确 |
| `rand()` / `std::mt19937` | `RandomXORShift32` | 相同种子不同平台序列一致 |

### Task 6.2 性能基准
- 编写一个临时 benchmark（可放在 `tests/` 或单独可执行文件）：
  - 创建 10,000 个实体，每帧更新 `GlobalTransform`。
  - 记录标量路径的平均帧时间。
  - 若帧时间 > 16ms，标记为后续 SIMD 批量优化的动机。

### Task 6.3 文档同步
- 更新 `_engine/source/math/AGENTS.md`：
  - 补充 `simd.h`、`half.h`、`ray.h`、`frustum.h`、`transform_component.h` 等新增文件。
  - 更新架构决策（列主序、对齐策略、SIMD 策略）。

---

## 风险与 Trade-off

| 风险 | 缓解措施 |
|------|----------|
| SIMD 抽象层过早引入导致代码膨胀 | 阶段 2 只实现最小可用接口（Add/Mul/Load/Store），不覆盖全部数学运算；上层系统仍走标量路径 |
| Float16 查表法增加二进制体积 | 查表数据用 `static const` 存储，仅 ~10KB；若体积敏感可换直接位运算（稍慢） |
| ECS Transform 组件归属争议 | 若 core 模块不允许依赖 MathLib，则把组件定义下沉到 math 模块，core 只存类型前向声明 |
| 编译器浮点开关影响全局性能 | 只在 MathLib 目标上设置严格 FP，不强制引擎其他模块；提供 `ENTELECHY_MATH_STRICT_FP` 选项 |
