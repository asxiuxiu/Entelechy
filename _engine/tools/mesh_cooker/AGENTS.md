# MeshCooker 工具模块

> 路径：`_engine/tools/mesh_cooker`

## 一句话职责

离线 glTF cook 工具：用 cgltf 解析 glTF 2.0 场景，输出引擎 `.emesh` 二进制网格与 `scene.json` 场景清单（渲染管线阶段 3b）。

## 关键文件
| 文件 | 职责 |
|------|------|
| `private/main.cpp` | 全部逻辑：CLI、cgltf 解析、accessor 解码交错化、`.emesh` 写出、node 树世界变换烘焙、`scene.json` 手写 emit、统计对账 |
| `CMakeLists.txt` | `entelechy_module(NAME MeshCooker TYPE EXECUTABLE NO_TESTS)`，`PRIVATE_DEPS AssetLib cgltf::cgltf` |

## 用法

```bash
# 从仓库根运行（默认路径相对当前工作目录）
python scripts/build/build.py --debug
./build/bin/Debug/MeshCooker.exe [input.gltf] [output_dir]
# 默认输入 _content/sponza/NewSponza_Main_glTF_003.gltf
# 默认输出 _content/sponza/cooked/（meshes/*.emesh + scene.json）
```

退出码：0 = 全部成功；2 = 有 primitive 被跳过（详见 stderr 告警）；1 = 致命错误（解析/写盘失败）。

## 架构决策
- **格式单一事实来源**：链接 [Asset 模块](../../source/asset/AGENTS.md)，直接复用 `MeshVertex`/`MeshAsset`/`writeMeshFile()`，不另写序列化代码。
- **cgltf 经 Conan 引入**（`cgltf/1.13`），与 stb 同构（根 `conanfile.py` + `find_package`）。
- **accessor 解码**用 `cgltf_accessor_read_float/read_index`（自动处理 byteStride/归一化）；sparse accessor 告警跳过；缺 NORMAL/TEXCOORD_0/TANGENT 填默认值并告警；无索引 primitive 生成顺序索引。
- **去重**：`.emesh` 按 `(meshIndex, primIndex)` 定名，每唯一 primitive 只写一次；`scene.json` 按 node 引用计数（实体数 ≥ .emesh 数）。
- **世界变换**用 `cgltf_node_transform_world()`，输出列主序 16 float，与引擎 `Mat4::m[16]` 布局一致，场景清单原样输出。
- **材质字段仅占位**（glTF material name），阶段 4 才做材质/纹理 cook。
- **cooked 产物不入 git**：可由 cooker 重新生成且体积大；`.gitignore` 现有 `_content/*` 规则已覆盖，无需例外。

## 依赖关系
- 向上依赖：
  - [Asset 模块](../../source/asset/AGENTS.md)（mesh 格式定义与 writer；AssetLib 传递链接 CoreLib）
  - cgltf（Conan `cgltf/1.13`，PRIVATE）
- 被依赖：无（独立可执行工具；根 `CMakeLists.txt` 的 launcher 自动链接循环会跳过 EXECUTABLE 类型模块）

## 技术债务

> 统一维护于 [TODO.md](../../../TODO.md)。当前无本模块条目。

## 测试
- 无单元测试（`NO_TESTS`）。验收方式：对 Sponza 资产实跑 + 统计对账（glTF 声明计数 vs 实际写出）+ 抽查 `.emesh` 经 [Asset 模块](../../source/asset/AGENTS.md) 的 loader 格式读回。
