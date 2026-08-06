# MeshCooker 工具模块

> 路径：`_engine/tools/mesh_cooker`

## 一句话职责

离线 glTF cook 工具：用 cgltf 解析 glTF 2.0 场景，输出引擎 `.emesh` 二进制网格、`.emat` 材质 JSON（渲染管线阶段 4a）与 `scene.json` 场景清单（渲染管线阶段 3b）。

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
# 默认输出 _content/sponza/cooked/（meshes/*.emesh + materials/*.emat + scene.json）
```

> ⚠️ 不要双击 exe 运行：默认输入/输出均按 cwd 相对解析，双击会在 `build/bin/Debug/` 下误建 `_content/` 空目录并因找不到输入 glTF 而失败。如需在其他 cwd 运行，显式传 `[input.gltf] [output_dir]` 两个参数。

退出码：0 = 全部成功；2 = 有 primitive 被跳过（详见 stderr 告警）；1 = 致命错误（解析/写盘失败）。

## 架构决策
- **格式单一事实来源**：链接 [Asset 模块](../../source/asset/AGENTS.md)，直接复用 `MeshVertex`/`MeshAsset`/`writeMeshFile()`，不另写序列化代码。
- **cgltf 经 Conan 引入**（`cgltf/1.13`），与 stb 同构（根 `conanfile.py` + `find_package`）。
- **accessor 解码**用 `cgltf_accessor_read_float/read_index`（自动处理 byteStride/归一化）；sparse accessor 告警跳过；缺 NORMAL/TEXCOORD_0/TANGENT 填默认值并告警；无索引 primitive 生成顺序索引。
- **UV V 翻转（阶段 4b，D2）**：写 `.emesh` 时 `v = 1.0f - v`——glTF UV 原点左上 vs OpenGL 底左；翻转属几何侧约定（`.emesh` 是 glTF 派生格式），纹理 loader 不动（`stbi_set_flip_vertically` 全局开关会污染 demo 棋盘格等既有用途）。变更后必须重跑 cook。
- **去重**：`.emesh` 按 `(meshIndex, primIndex)` 定名，每唯一 primitive 只写一次；`scene.json` 按 node 引用计数（实体数 ≥ .emesh 数）。
- **世界变换**用 `cgltf_node_transform_world()`，输出列主序 16 float，与引擎 `Mat4::m[16]` 布局一致，场景清单原样输出；每实体附带该 primitive 的局部空间 AABB（`aabb_min`/`aabb_max`，与 `.emesh` 头内一致，游戏侧变换到世界空间做视锥剔除，阶段 3c）。
- **材质 cook（阶段 4a）**：每 glTF 材质导出一个 `materials/<name>.emat` 手写 JSON（贴图内容路径 + pbrMetallicRoughness factor + alphaMode/alphaCutoff/doubleSided，schema 单一事实来源为 `main.cpp` 的 `cookMaterials()` 注释与 [Asset 模块](../../source/asset/AGENTS.md) 的 MaterialAssetLoader）；贴图 URI 相对 `.gltf` 目录解析、百分号解码后转成相对 `_content/` 的内容路径（如 `sponza/textures/x.png`）；材质名清洗为 `[A-Za-z0-9_-]` 文件干名；`scene.json` 实体 `material` 字段写 `.emat` 清单相对路径，无材质 primitive 写空串并告警；结尾打印 mask/blend/doubleSided/缺 baseColor 统计。
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
