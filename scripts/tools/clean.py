#!/usr/bin/env python3
"""
清理 Entelechy 生成的构建目录和文件。
跨平台入口（推荐）。

用法:
    python scripts/tools/clean.py          # 标准清理：删除 build/
    python scripts/tools/clean.py --deep   # 深度清理：删除 build/ + .conan/ + .conan_home/
"""

import argparse
import os
import shutil
import stat
import sys
from pathlib import Path

# Allow importing env_config from the same directory.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from env_config import get_project_root, load_env_config


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
        # onerror 兼容 Python 3.10+；onexc 从 3.12 起才可用。
        shutil.rmtree(path, onerror=_handle_remove_error)
    else:
        print(f"Already clean: {path} does not exist.")


def clean(deep: bool = False):
    project_root = get_project_root()
    config = load_env_config()
    build_dir = project_root / "build"
    conan_dir = project_root / ".conan"
    conan_home_dir = project_root / config["conan"].get("home", ".conan_home")

    remove_dir(build_dir)

    if deep:
        remove_dir(conan_dir)
        remove_dir(conan_home_dir)
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
        help="Also remove .conan/ and the project-local Conan home directory.",
    )
    args = parser.parse_args()
    sys.exit(clean(deep=args.deep))
