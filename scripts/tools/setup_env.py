#!/usr/bin/env python3
"""
Bootstrap the project-local Python virtual environment and install pinned
build dependencies (Conan, CMake, Ninja) according to configs/environment.json.

Usage:
    python scripts/tools/setup_env.py
"""

import os
import subprocess
import sys
from pathlib import Path

# Allow importing env_config from the same directory.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from env_config import get_project_root, load_env_config

PROJECT_ROOT = get_project_root()
VENV_DIR = PROJECT_ROOT / ".venv"


def get_venv_bin(name):
    """Return the path to an executable inside the venv."""
    if sys.platform == "win32":
        return VENV_DIR / "Scripts" / f"{name}.exe"
    return VENV_DIR / "bin" / name


def get_venv_package_version(name):
    """Return the installed version of a package in the venv, or None."""
    python = get_venv_bin("python")
    if not python.exists():
        return None
    try:
        result = subprocess.run(
            [str(python), "-c", f"import importlib.metadata; print(importlib.metadata.version('{name}'))"],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except Exception:
        pass
    return None


def _parse_version(version_str):
    """Parse a version string into a tuple of ints (ignore non-numeric suffixes)."""
    parts = []
    for part in version_str.split("."):
        numeric = ""
        for ch in part:
            if ch.isdigit():
                numeric += ch
            else:
                break
        if numeric:
            parts.append(int(numeric))
    return tuple(parts)


def version_satisfies(installed, required):
    """Check whether installed version matches required (major.minor.patch)."""
    if not installed:
        return False
    return _parse_version(installed)[:3] == _parse_version(required)[:3]


def run(cmd, cwd=None, env=None):
    """Run a command and forward stdout/stderr. Exit on failure."""
    print(f"[Setup] {' '.join(str(c) for c in cmd)}")
    result = subprocess.run(cmd, cwd=cwd, env=env)
    if result.returncode != 0:
        sys.exit(result.returncode)


def ensure_venv():
    """Create the project-local venv if it does not yet exist."""
    if not VENV_DIR.exists():
        print("[Setup] Creating virtual environment...")
        run([sys.executable, "-m", "venv", str(VENV_DIR)], cwd=PROJECT_ROOT)

    python = get_venv_bin("python")
    if not python.exists():
        print(f"[Setup] Error: venv created but {python} not found.")
        sys.exit(1)
    return python


def install_python_deps(config):
    """Install pinned Conan, CMake and Ninja into the venv (idempotent)."""
    python = get_venv_bin("python")
    pip = get_venv_bin("pip")

    print("[Setup] Upgrading pip...")
    run([str(python), "-m", "pip", "install", "--upgrade", "pip", "setuptools"], cwd=PROJECT_ROOT)

    conan_version = config["conan"]["version"]
    installed_conan = get_venv_package_version("conan")
    if version_satisfies(installed_conan, conan_version):
        print(f"[Setup] conan {installed_conan} already installed (required {conan_version}), skipping.")
    else:
        print(f"[Setup] Installing conan=={conan_version}...")
        run([str(pip), "install", f"conan=={conan_version}"], cwd=PROJECT_ROOT)

    cmake_cfg = config.get("cmake", {})
    if cmake_cfg.get("install_in_venv", False):
        cmake_version = cmake_cfg["version"]
        installed_cmake = get_venv_package_version("cmake")
        if version_satisfies(installed_cmake, cmake_version):
            print(f"[Setup] cmake {installed_cmake} already installed (required {cmake_version}), skipping.")
        else:
            print(f"[Setup] Installing cmake=={cmake_version} into venv...")
            run([str(pip), "install", f"cmake=={cmake_version}"], cwd=PROJECT_ROOT)

    ninja_cfg = config.get("ninja", {})
    if ninja_cfg.get("install_in_venv", False):
        installed_ninja = get_venv_package_version("ninja")
        ninja_version = ninja_cfg.get("version")
        if installed_ninja and (not ninja_version or version_satisfies(installed_ninja, ninja_version)):
            print(f"[Setup] ninja {installed_ninja} already installed, skipping.")
        else:
            if ninja_version:
                print(f"[Setup] Installing ninja=={ninja_version} into venv...")
                run([str(pip), "install", f"ninja=={ninja_version}"], cwd=PROJECT_ROOT)
            else:
                print("[Setup] Installing ninja into venv...")
                run([str(pip), "install", "ninja"], cwd=PROJECT_ROOT)


def ensure_conan_home(config):
    """Create the project-local Conan home directory."""
    conan_home_rel = config["conan"].get("home", ".conan_home")
    conan_home = PROJECT_ROOT / conan_home_rel
    if not conan_home.exists():
        print(f"[Setup] Creating project-local Conan home: {conan_home}")
        (conan_home / "profiles").mkdir(parents=True, exist_ok=True)
    return conan_home


def initialize_conan(conan_cmd):
    """Run a lightweight Conan command so it creates settings.yml in the new home."""
    print("[Setup] Initializing Conan home...")
    subprocess.run([conan_cmd, "profile", "list"], check=False)


def write_env_hint(conan_home):
    """Write a small hint file next to the venv documenting CONAN_HOME."""
    hint = VENV_DIR / "ENTELECHY_ENV.txt"
    hint.write_text(
        f"Project-local development environment.\n"
        f"CONAN_HOME={conan_home}\n"
        f"Run activate_entelechy to set CONAN_HOME in your shell.\n",
        encoding="utf-8",
    )


def write_activation_scripts(conan_home):
    """Generate shell activation scripts that set CONAN_HOME."""
    conan_home_str = str(conan_home.resolve())

    if sys.platform == "win32":
        # Batch wrapper for cmd.exe
        batch = VENV_DIR / "Scripts" / "activate_entelechy.bat"
        batch.write_text(
            "@echo off\n"
            f'call "%~dp0activate.bat"\n'
            f"set CONAN_HOME={conan_home_str}\n"
            'echo [Entelechy] CONAN_HOME=%CONAN_HOME%\n',
            encoding="utf-8",
        )
        # PowerShell wrapper
        ps1 = VENV_DIR / "Scripts" / "Activate_entelechy.ps1"
        ps1.write_text(
            "$activate = Join-Path $PSScriptRoot 'Activate.ps1'\n"
            "\u0026 $activate\n"
            f"$env:CONAN_HOME = '{conan_home_str}'\n"
            "Write-Host \"[Entelechy] CONAN_HOME=$env:CONAN_HOME\"\n",
            encoding="utf-8",
        )
        # Git Bash / MSYS wrapper
        sh = VENV_DIR / "Scripts" / "activate_entelechy"
        sh.write_text(
            "#!/bin/sh\n"
            '# Entelechy project-local environment activation for Git Bash\n'
            '. "$(dirname "${BASH_SOURCE[0]}")/activate"\n'
            f"export CONAN_HOME='{conan_home_str}'\n"
            'echo "[Entelechy] CONAN_HOME=$CONAN_HOME"\n',
            encoding="utf-8",
        )
        os.chmod(sh, 0o755)
        print(
            f"[Setup] Created activation scripts: {batch.name}, {ps1.name}, {sh.name}"
        )
    else:
        shell = VENV_DIR / "bin" / "activate_entelechy"
        shell.write_text(
            "#!/bin/sh\n"
            '# Entelechy project-local environment activation\n'
            '. "$(dirname "${BASH_SOURCE[0]}")/activate"\n'
            f"export CONAN_HOME='{conan_home_str}'\n"
            'echo "[Entelechy] CONAN_HOME=$CONAN_HOME"\n',
            encoding="utf-8",
        )
        os.chmod(shell, 0o755)
        print(f"[Setup] Created activation script: {shell.name}")


def ensure_local_config():
    """Copy environment.local.json template if the local config is missing."""
    template = PROJECT_ROOT / "configs" / "environment.local.json.template"
    local = PROJECT_ROOT / "configs" / "environment.local.json"
    if template.exists() and not local.exists():
        print(f"[Setup] Creating {local} from template...")
        local.write_text(template.read_text(encoding="utf-8"), encoding="utf-8")
        print("[Setup] Please edit it if your VS installation is in a non-default path.")


def print_versions():
    """Print the versions of the tools we just installed."""
    conan = get_venv_bin("conan")
    if conan.exists():
        print("[Setup] Installed Conan:")
        run([str(conan), "--version"], cwd=PROJECT_ROOT)

    cmake = get_venv_bin("cmake")
    if cmake.exists():
        print("[Setup] Installed CMake:")
        run([str(cmake), "--version"], cwd=PROJECT_ROOT)

    ninja = get_venv_bin("ninja")
    if ninja.exists():
        print("[Setup] Installed Ninja:")
        run([str(ninja), "--version"], cwd=PROJECT_ROOT)


def main():
    config = load_env_config()

    print(f"[Setup] Project root: {PROJECT_ROOT}")
    print(f"[Setup] Virtual env:  {VENV_DIR}")

    ensure_local_config()
    ensure_venv()
    install_python_deps(config)
    conan_home = ensure_conan_home(config)
    conan = get_venv_bin("conan")
    if conan.exists():
        initialize_conan(str(conan))
    write_env_hint(conan_home)
    write_activation_scripts(conan_home)
    print_versions()

    print("[Setup] Done. You can now run build tasks.")


if __name__ == "__main__":
    main()
