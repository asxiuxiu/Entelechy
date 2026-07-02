#!/usr/bin/env python3
"""
Bootstrap the project-local Python virtual environment and install pinned
build dependencies (Conan, etc.).

Usage:
    python scripts/tools/setup_env.py
"""

import os
import platform
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
VENV_DIR = PROJECT_ROOT / ".venv"
REQUIREMENTS = PROJECT_ROOT / "requirements.txt"


def get_venv_bin(name):
    """Return the path to an executable inside the venv."""
    if sys.platform == "win32":
        return VENV_DIR / "Scripts" / f"{name}.exe"
    return VENV_DIR / "bin" / name


def run(cmd, cwd=None):
    """Run a command and forward stdout/stderr. Exit on failure."""
    print(f"[Setup] {' '.join(str(c) for c in cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        sys.exit(result.returncode)


def main():
    print(f"[Setup] Project root: {PROJECT_ROOT}")
    print(f"[Setup] Virtual env:  {VENV_DIR}")

    # Create venv if missing
    if not VENV_DIR.exists():
        print("[Setup] Creating virtual environment...")
        run([sys.executable, "-m", "venv", str(VENV_DIR)], cwd=PROJECT_ROOT)

    python = get_venv_bin("python")
    pip = get_venv_bin("pip")

    if not python.exists():
        print(f"[Setup] Error: venv created but {python} not found.")
        sys.exit(1)

    # Upgrade pip/setuptools to avoid old wheel bugs
    print("[Setup] Upgrading pip...")
    run([str(python), "-m", "pip", "install", "--upgrade", "pip", "setuptools"], cwd=PROJECT_ROOT)

    # Install pinned dependencies
    if REQUIREMENTS.exists():
        print(f"[Setup] Installing dependencies from {REQUIREMENTS}...")
        run([str(pip), "install", "-r", str(REQUIREMENTS)], cwd=PROJECT_ROOT)
    else:
        print(f"[Setup] Warning: {REQUIREMENTS} not found, skipping package install.")

    conan = get_venv_bin("conan")
    if conan.exists():
        print("[Setup] Installed Conan version:")
        run([str(conan), "--version"], cwd=PROJECT_ROOT)

    print("[Setup] Done. You can now run build tasks.")


if __name__ == "__main__":
    main()
