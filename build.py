#!/usr/bin/env python3
"""
Entelechy Cross-Platform Build Script
Usage: python build.py [ --config <json> ] [ --debug | --release ]
"""

import argparse
import subprocess
import sys
from pathlib import Path


def run(cmd):
    """Run a command and forward stdout/stderr. Exit on failure."""
    print(f"[Build] {' '.join(cmd)}")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        sys.exit(result.returncode)


def main():
    parser = argparse.ArgumentParser(description='Entelechy Build Script')
    parser.add_argument('--config', default='configs/full_build.json',
                        help='Path to build config JSON')
    parser.add_argument('--debug', action='store_true',
                        help='Build Debug configuration')
    parser.add_argument('--release', action='store_true',
                        help='Build Release configuration')
    parser.add_argument('--build', action='store_true',
                        help='Also compile after CMake configuration')
    args = parser.parse_args()

    build_type = 'Debug' if args.debug else 'Release'
    if args.release:
        build_type = 'Release'

    config_path = args.config
    print(f"[Build] Config: {config_path}")
    print(f"[Build] Type: {build_type}")

    # Step 0: Install dependencies via Conan
    print("[Build] Step 0: Installing dependencies via Conan...")
    conan_cmd = [
        "conan", "install", ".",
        "--output-folder=build/conan",
        "--build=missing",
        "-s", f"build_type={build_type}"
    ]
    run(conan_cmd)

    # Step 1: Generate Source Forest
    print("[Build] Step 1: Generating Source Forest...")
    run([sys.executable, "launch/generator.py", "--config", config_path])

    # Step 2: Run CMake
    print("[Build] Step 2: Running CMake...")
    cmake_cmd = ["cmake", "-S", ".", "-B", "build"]
    if sys.platform == "win32":
        cmake_cmd += [
            "-G", "Visual Studio 17 2022",
            "-A", "x64",
            "-DCMAKE_CONFIGURATION_TYPES=Debug;Release"
        ]
    else:
        cmake_cmd += [f"-DCMAKE_BUILD_TYPE={build_type}"]
    cmake_cmd += ["-DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake"]
    cmake_cmd += ["-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
    run(cmake_cmd)

    # Step 3: Build (optional)
    if args.build:
        print(f"[Build] Step 3: Building {build_type}...")
        run(["cmake", "--build", "build", "--config", build_type, "--parallel"])
        print("[Build] Success! Output: build/bin/")
    else:
        print("[Build] Done. (Use --build to also compile)")


if __name__ == '__main__':
    main()
