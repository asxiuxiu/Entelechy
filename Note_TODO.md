# 笔记整理待办

> 本文件用于收集项目中遇到的不熟悉概念、术语或需要后续深入研究的知识点。Agent 在记录前会优先通过 `vault-context` skill 搜索用户的 Obsidian 知识库，若知识库中无对应内容，再追加到此处，方便后续统一整理。
>
> **注意**：技术债务（代码中已知的缺陷、临时约束、待优化项）统一记录在项目根目录 `TODO.md` 中，不在此文件重复。

## 待整理概念

- **PBR/GGX 实现细节**（2026-08-06，渲染阶段 5 调研发现）：vault 已有 Blinn-Phong、法线矩阵（逆转置）、数据通道与更新频率三篇光照基础笔记（`Notes/计算机图形学/光照与数据流/`），但没有 Cook-Torrance GGX 的具体实现笔记（D 项 GGX 分布、F 项 Schlick 近似、G 项 Smith 几何项的公式与代码形态、metallic-roughness 工作流下 F0 的推导）。阶段 5 实现时只能参照 learnopengl.com PBR 等外部资料。建议补一篇「从 Blinn-Phong 到 GGX」笔记衔接既有《让像素响应光》。
- **metallic-roughness 贴图打包约定**（同上）：glTF 约定 G=roughness、B=metallic，以及 factor×texture 相乘语义，vault 无对应整理；与《纹理系统/多重纹理与材质》主题相邻，可并入或单写。 

## 已整理概念

- 
