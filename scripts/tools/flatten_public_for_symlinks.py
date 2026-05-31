#!/usr/bin/env python3
"""
将各模块 public/<module>/ 下的内容物理上移到 public/。
这是为"目录链接"方案做准备：
  物理: ecs/public/event/...
  链接: build/include/ecs -> ecs/public
  引用: #include "ecs/event/..."
"""

import os
import shutil
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
MODULE_ROOTS = [PROJECT_ROOT / "_engine" / "source", PROJECT_ROOT / "_game" / "source"]

def flatten_module(public_dir: Path, nested_dir: Path):
    """递归将 nested_dir 下的内容合并到 public_dir。"""
    for item in nested_dir.iterdir():
        dest = public_dir / item.name
        if dest.exists():
            if item.is_dir() and dest.is_dir():
                flatten_module(dest, item)
                continue
            else:
                raise FileExistsError(
                    f"跨模块或内部冲突: 源={item}, 目标={dest}"
                )
        shutil.move(str(item), str(dest))
    nested_dir.rmdir()

def main():
    modules = []
    for root in MODULE_ROOTS:
        if not root.exists():
            continue
        for mod_dir in root.iterdir():
            if not mod_dir.is_dir():
                continue
            public_dir = mod_dir / "public"
            nested_dir = public_dir / mod_dir.name
            if nested_dir.exists() and nested_dir.is_dir():
                modules.append((mod_dir.name, public_dir, nested_dir))

    if not modules:
        print("没有找到需要扁平化的模块。")
        return

    # 先检查全局冲突
    all_dest_files = {}
    for name, public_dir, nested_dir in modules:
        for item in nested_dir.rglob("*"):
            if item.is_file():
                rel = item.relative_to(nested_dir)
                key = str(rel).replace("\\", "/")
                if key in all_dest_files:
                    other = all_dest_files[key]
                    raise FileExistsError(
                        f"全局文件冲突: '{key}' 出现在 {name} 和 {other}"
                    )
                all_dest_files[key] = name

    print(f"将扁平化 {len(modules)} 个模块...")
    for name, public_dir, nested_dir in modules:
        print(f"  [{name}] {nested_dir} -> {public_dir}")
        flatten_module(public_dir, nested_dir)

    print("✅ 扁平化完成。")

if __name__ == "__main__":
    main()
