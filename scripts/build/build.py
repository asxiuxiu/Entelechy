#!/usr/bin/env python3
"""
Entelechy Cross-Platform Build Script
Usage: python build.py [ --config <json> ] [ --debug | --release ]
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


def run(cmd):
    """Run a command and forward stdout/stderr. Exit on failure."""
    print(f"[Build] {' '.join(cmd)}")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        sys.exit(result.returncode)


def get_cmake_version():
    """Detect installed CMake version as (major, minor, patch) tuple."""
    try:
        result = subprocess.run(
            ["cmake", "--version"], capture_output=True, text=True, check=True
        )
        match = re.search(r"cmake version (\d+)\.(\d+)\.(\d+)", result.stdout)
        if match:
            return tuple(int(x) for x in match.groups())
    except Exception:
        pass
    return (3, 20, 0)


def get_supported_msvc_versions():
    """Parse ~/.conan2/settings.yml to get the list of supported MSVC versions."""
    settings_path = Path.home() / ".conan2" / "settings.yml"
    if not settings_path.exists():
        return None

    content = settings_path.read_text(encoding="utf-8")
    lines = content.splitlines()

    in_msvc = False
    msvc_indent = None
    version_line_idx = -1

    for i, line in enumerate(lines):
        msvc_match = re.match(r'^(\s*)msvc:\s*$', line)
        if msvc_match:
            in_msvc = True
            msvc_indent = len(msvc_match.group(1))
            continue

        if in_msvc:
            current_indent = len(re.match(r'^(\s*)', line).group(1))
            stripped = line.strip()
            # If indent returns to msvc level (or less) and it's not a continuation, we've left the block
            if current_indent <= msvc_indent and stripped and not stripped.startswith('#'):
                if 'version' not in stripped:
                    break

            if re.match(r'^\s+version:\s*', line):
                version_line_idx = i
                break

    if version_line_idx < 0:
        return None

    version_line = lines[version_line_idx].strip()
    # Inline list: version: [170, 180, ...]
    list_match = re.search(r'\[([^\]]*)\]', version_line)
    if list_match:
        items = [v.strip().strip('"').strip("'") for v in list_match.group(1).split(',')]
        return [v for v in items if v]

    # Multi-line list
    versions = []
    base_indent = len(re.match(r'^(\s*)', lines[version_line_idx]).group(1))
    for j in range(version_line_idx + 1, len(lines)):
        line = lines[j]
        current_indent = len(re.match(r'^(\s*)', line).group(1))
        if current_indent <= base_indent:
            break
        match = re.match(r'^\s+-\s*(\S+)', line)
        if match:
            versions.append(match.group(1).strip('"').strip("'"))

    return versions if versions else None


def pick_msvc_version(supported_versions):
    """Choose the best MSVC version from the supported list."""
    if not supported_versions:
        return "193"
    # Prefer 194 (newer VS2022), otherwise fall back to the maximum available
    if "194" in supported_versions:
        return "194"
    numeric = [v for v in supported_versions if v.isdigit()]
    if numeric:
        return max(numeric, key=int)
    return supported_versions[-1]


def prepare_conan_profile():
    """Ensure a project-local Conan profile exists and is valid for current Conan."""
    profile_dir = Path(".conan/profiles")
    profile_path = profile_dir / "default"

    supported = get_supported_msvc_versions()
    target_version = pick_msvc_version(supported)

    if profile_path.exists():
        content = profile_path.read_text(encoding="utf-8")
        match = re.search(r'compiler\.version=(\d+)', content)
        if match:
            current_version = match.group(1)
            if supported and current_version not in supported:
                # Profile outdated (Conan dropped the old version), update it
                new_content = re.sub(
                    r'compiler\.version=\d+',
                    f'compiler.version={target_version}',
                    content
                )
                profile_path.write_text(new_content, encoding="utf-8")
                print(f"[Build] Updated local Conan profile: {current_version} -> {target_version}")
            return str(profile_path)

    # Generate new profile
    profile_dir.mkdir(parents=True, exist_ok=True)
    profile_content = f"""[settings]
arch=x86_64
build_type=Release
compiler=msvc
compiler.cppstd=14
compiler.runtime=dynamic
compiler.version={target_version}
os=Windows
"""
    profile_path.write_text(profile_content, encoding="utf-8")
    print(f"[Build] Generated local Conan profile (compiler.version={target_version})")
    return str(profile_path)


def patch_conan_toolchain():
    """Patch Conan toolchain for CMake < 3.26 CMP0091 compatibility."""
    cmake_version = get_cmake_version()
    if cmake_version >= (3, 26, 0):
        return  # Newer CMake handles this correctly

    toolchain_path = Path("build/conan/conan_toolchain.cmake")
    if not toolchain_path.exists():
        return

    content = toolchain_path.read_text(encoding="utf-8")
    old_check = "cmake_policy(GET CMP0091 POLICY_CMP0091)"
    new_check = "cmake_policy(SET CMP0091 NEW)\ncmake_policy(GET CMP0091 POLICY_CMP0091)"

    if new_check in content:
        return  # Already patched

    if old_check not in content:
        print("[Build] Warning: Conan toolchain format changed; CMP0091 patch not applied. "
              "If CMake < 3.26 fails, please update CMake.")
        return

    content = content.replace(old_check, new_check)
    toolchain_path.write_text(content, encoding="utf-8")
    print("[Build] Patched conan_toolchain.cmake for CMP0091 compatibility")


def prepare_cmake_file_api():
    """Create CMake File API query so that VS Code CMake Tools can pick up
    IntelliSense info even when using the Visual Studio generator."""
    query_dir = Path("build/.cmake/api/v1/query/client-entelechy-build")
    query_dir.mkdir(parents=True, exist_ok=True)
    query_file = query_dir / "query.json"
    query = {
        "requests": [
            {"kind": "cache", "version": 2},
            {"kind": "codemodel", "version": 2},
            {"kind": "toolchains", "version": 1}
        ]
    }
    query_file.write_text(json.dumps(query), encoding="utf-8")
    print("[Build] Prepared CMake File API query for IDE IntelliSense")


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
    parser.add_argument('--shipping', action='store_true',
                        help='Shipping build (strips debug/info logs)')
    args = parser.parse_args()

    build_type = 'Debug' if args.debug else 'Release'
    if args.release:
        build_type = 'Release'

    config_path = args.config
    print(f"[Build] Config: {config_path}")
    print(f"[Build] Type: {build_type}")

    # Prepare project-local Conan profile (adapts to current Conan version)
    profile_path = prepare_conan_profile()

    # Step 0: Install dependencies via Conan
    print("[Build] Step 0: Installing dependencies via Conan...")
    conan_cmd = [
        "conan", "install", ".",
        "--output-folder=build/conan",
        "--build=missing",
        f"--profile:host={profile_path}",
        f"--profile:build={profile_path}",
        "-s", f"build_type={build_type}"
    ]
    run(conan_cmd)

    # Patch toolchain for old CMake compatibility (no-op on CMake >= 3.26)
    patch_conan_toolchain()

    # Step 1: Generate Source Forest
    print("[Build] Step 1: Generating Source Forest...")
    run([sys.executable, "launch/generator.py", "--config", config_path])

    # Prepare CMake File API query for IDE IntelliSense (Visual Studio generator
    # does not generate compile_commands.json, so we rely on File API replies)
    prepare_cmake_file_api()

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
    cmake_cmd += ["-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"]
    cmake_cmd += ["-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
    if args.shipping:
        cmake_cmd += ["-DSHIPPING_BUILD=ON"]
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
