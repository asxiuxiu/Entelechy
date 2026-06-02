#!/usr/bin/env python3
"""
清理 Entelechy 生成的构建目录和文件。
跨平台入口（推荐）。

用法:
    python scripts/tools/clean.py          # 标准清理：删除 build/
    python scripts/tools/clean.py --deep   # 深度清理：删除 build/ + .conan/
"""

import argparse
import os
import shutil
import stat
import sys
from pathlib import Path


def _handle_remove_error(func, path, exc_info):
    """Windows 上处理被占用/只读文件的删除错误。"""
    try:
        os.chmod(path, stat.S_IWRITE)
        func(path)
    except OSError as e:
        print(f"Warning: could not remove '{path}': {e}")


def remove_dir(path: Path):
    if path.exists():
        print(f"Removing: {path}")
        shutil.rmtree(path, onexc=_handle_remove_error)
    else:
        print(f"Already clean: {path} does not exist.")


def clean(deep: bool = False):
    project_root = Path(__file__).parent.parent.parent.resolve()
    build_dir = project_root / "build"
    conan_dir = project_root / ".conan"

    remove_dir(build_dir)

    if deep:
        remove_dir(conan_dir)
        print("Deep clean finished.")
    else:
        print("Standard clean finished.")

    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Clean generated build directories for Entelechy."
    )
    parser.add_argument(
        "--deep",
        action="store_true",
        help="Also remove .conan/ profile configuration directory.",
    )
    args = parser.parse_args()
    sys.exit(clean(deep=args.deep))
