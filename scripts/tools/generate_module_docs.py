#!/usr/bin/env python3
"""
generate_module_docs.py
为新增的模块目录生成 AGENTS.md 骨架模板。

用法：
    python scripts/tools/generate_module_docs.py

已存在 AGENTS.md 的模块默认跳过（加 --force 可覆盖）。
"""

import argparse
from pathlib import Path

MODULE_ROOTS = [
    "_engine/source",
    "_game/source",
]


def get_source_files(module_dir: Path):
    """返回模块中的源码文件列表（排除 CMakeLists.txt）"""
    files = []
    for f in sorted(module_dir.iterdir()):
        if f.is_file() and f.suffix in (".h", ".hpp", ".cpp", ".c"):
            files.append(f.name)
    return files


def generate_skeleton_doc(module_dir: Path, module_name: str, rel_path: str):
    actual_files = get_source_files(module_dir)
    file_rows = "\n".join(
        f"| `{f}` | <!-- TODO: 补充说明 --> |" for f in actual_files
    ) or "| <!-- 无源码文件 --> | <!-- --> |"

    return f"""# {module_name} 模块

> 路径：`{rel_path}`

## 一句话职责
<!-- TODO: 用一句话描述这个模块的核心职责 -->

## 关键文件
| 文件 | 职责 |
|------|------|
{file_rows}

## 重要入口
<!-- TODO: Agent 修改代码时最可能动的地方，用描述性语言 -->

## 依赖关系
- 向上依赖：
  - <!-- TODO: 本模块依赖的其他模块（如：[Window 模块](../window/AGENTS.md)） -->
- 被依赖：
  - <!-- TODO: 哪些模块依赖本模块 -->

## 架构决策 / 临时约束
<!-- TODO: 已知的技术债务、设计约束、待办事项 -->
"""


def main():
    parser = argparse.ArgumentParser(description="Generate module AGENTS.md skeletons")
    parser.add_argument("--force", action="store_true", help="覆盖已存在的 AGENTS.md")
    args = parser.parse_args()

    root = Path(".")
    generated = []
    skipped = []

    for module_root in MODULE_ROOTS:
        root_path = root / module_root
        if not root_path.exists():
            continue

        for module_dir in sorted(root_path.iterdir()):
            if not module_dir.is_dir():
                continue

            doc_path = module_dir / "AGENTS.md"
            rel_path = module_dir.relative_to(root).as_posix()
            module_name = module_dir.name

            if doc_path.exists() and not args.force:
                skipped.append(rel_path)
                continue

            content = generate_skeleton_doc(module_dir, module_name, rel_path)
            doc_path.write_text(content, encoding="utf-8")
            generated.append(rel_path)

    print(f"Generated {len(generated)} module docs:")
    for p in generated:
        print(f"  + {p}/AGENTS.md")

    if skipped:
        print(f"Skipped {len(skipped)} (already exists):")
        for p in skipped:
            print(f"  = {p}/AGENTS.md")


if __name__ == "__main__":
    main()
