#!/usr/bin/env python3
"""
Entelechy Cross-Platform Build Script
Usage: python build.py [ --config <json> ] [ --debug | --release ]
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

from setup_skills import setup_skills_link

PROJECT_ROOT = Path(__file__).resolve().parents[2]
VENV_DIR = PROJECT_ROOT / ".venv"


def get_project_tool(name):
    """Return the path to a tool inside the project venv, if it exists."""
    if sys.platform == "win32":
        candidate = VENV_DIR / "Scripts" / f"{name}.exe"
    else:
        candidate = VENV_DIR / "bin" / name
    return candidate if candidate.exists() else None


def get_conan_cmd():
    """Return the conan command to use, preferring the project venv."""
    project_conan = get_project_tool("conan")
    if project_conan:
        return str(project_conan)
    return "conan"


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


def get_supported_compiler_versions(compiler_name):
    """Parse ~/.conan2/settings.yml to get supported versions for a compiler."""
    settings_path = Path.home() / ".conan2" / "settings.yml"
    if not settings_path.exists():
        return None

    content = settings_path.read_text(encoding="utf-8")
    lines = content.splitlines()

    in_compiler = False
    compiler_indent = None
    version_line_idx = -1

    for i, line in enumerate(lines):
        compiler_match = re.match(rf"^(\s*){re.escape(compiler_name)}:\s*$", line)
        if compiler_match:
            in_compiler = True
            compiler_indent = len(compiler_match.group(1))
            continue

        if in_compiler:
            current_indent = len(re.match(r"^(\s*)", line).group(1))
            stripped = line.strip()
            if (
                current_indent <= compiler_indent
                and stripped
                and not stripped.startswith("#")
            ):
                if "version" not in stripped:
                    break

            if re.match(r"^\s+version:\s*", line):
                version_line_idx = i
                break

    if version_line_idx < 0:
        return None

    version_line = lines[version_line_idx].strip()
    list_match = re.search(r"\[([^\]]*)\]", version_line)
    if list_match:
        items = [
            v.strip().strip('"').strip("'") for v in list_match.group(1).split(",")
        ]
        return [v for v in items if v]

    versions = []
    base_indent = len(re.match(r"^(\s*)", lines[version_line_idx]).group(1))
    for j in range(version_line_idx + 1, len(lines)):
        line = lines[j]
        current_indent = len(re.match(r"^(\s*)", line).group(1))
        if current_indent <= base_indent:
            break
        match = re.match(r"^\s+-\s*(\S+)", line)
        if match:
            versions.append(match.group(1).strip('"').strip("'"))

    return versions if versions else None


def parse_version_list(versions):
    """Parse version strings into comparable tuples of ints."""
    parsed = []
    for v in versions:
        try:
            parsed.append((tuple(int(x) for x in v.split(".")), v))
        except ValueError:
            continue
    return parsed


def pick_nearest_version(supported_versions, detected_version):
    """Pick the nearest supported version that is <= detected version."""
    if not supported_versions:
        return detected_version
    if detected_version in supported_versions:
        return detected_version

    parsed_supported = parse_version_list(supported_versions)
    parsed_detected = parse_version_list([detected_version])
    if not parsed_supported or not parsed_detected:
        return supported_versions[-1]

    detected_tuple = parsed_detected[0][0]
    candidates = [(t, v) for t, v in parsed_supported if t <= detected_tuple]
    if candidates:
        return max(candidates, key=lambda x: x[0])[1]
    return min(parsed_supported, key=lambda x: x[0])[1]


def pick_msvc_version(supported_versions):
    """Choose the best MSVC version from the supported list."""
    if not supported_versions:
        return "193"
    if "194" in supported_versions:
        return "194"
    numeric = [v for v in supported_versions if v.isdigit()]
    if numeric:
        return max(numeric, key=int)
    return supported_versions[-1]


def detect_apple_clang_version():
    """Detect Apple Clang version string suitable for Conan (e.g., 14.0)."""
    try:
        result = subprocess.run(
            ["clang", "--version"], capture_output=True, text=True, check=True
        )
        match = re.search(r"Apple clang version\s+(\d+\.\d+)", result.stdout)
        if match:
            return match.group(1)
    except Exception:
        pass
    return "14.0"


def detect_gcc_version():
    """Detect GCC major version suitable for Conan."""
    try:
        result = subprocess.run(
            ["gcc", "--version"], capture_output=True, text=True, check=True
        )
        match = re.search(r"gcc\s+.*?\s+(\d+\.\d+)", result.stdout, re.IGNORECASE)
        if match:
            return match.group(1).split(".")[0]
    except Exception:
        pass
    return "11"


def detect_linux_clang_version():
    """Detect Linux Clang version string suitable for Conan."""
    try:
        result = subprocess.run(
            ["clang", "--version"], capture_output=True, text=True, check=True
        )
        match = re.search(r"clang version\s+(\d+\.\d+)", result.stdout)
        if match:
            return match.group(1).split(".")[0]
    except Exception:
        pass
    return "15"


def get_default_platform_profile():
    """Return default Conan profile settings for the current host platform."""
    if sys.platform == "win32":
        supported = get_supported_compiler_versions("msvc")
        version = pick_msvc_version(supported)
        return {
            "compiler": "msvc",
            "compiler.version": version,
            "compiler.cppstd": "20",
            "compiler.runtime": "dynamic",
            "os": "Windows",
            "arch": "x86_64",
        }

    if sys.platform == "darwin":
        supported = get_supported_compiler_versions("apple-clang")
        detected = detect_apple_clang_version()
        version = pick_nearest_version(supported, detected) if supported else detected
        return {
            "compiler": "apple-clang",
            "compiler.version": version,
            "compiler.cppstd": "20",
            "compiler.libcxx": "libc++",
            "os": "Macos",
            "arch": "x86_64",
        }

    # Linux: prefer GCC, fallback to Clang if GCC unavailable
    supported_gcc = get_supported_compiler_versions("gcc")
    if supported_gcc is not None:
        detected = detect_gcc_version()
        version = pick_nearest_version(supported_gcc, detected)
        return {
            "compiler": "gcc",
            "compiler.version": version,
            "compiler.cppstd": "20",
            "compiler.libcxx": "libstdc++11",
            "os": "Linux",
            "arch": "x86_64",
        }

    supported_clang = get_supported_compiler_versions("clang")
    detected = detect_linux_clang_version()
    version = (
        pick_nearest_version(supported_clang, detected) if supported_clang else detected
    )
    return {
        "compiler": "clang",
        "compiler.version": version,
        "compiler.cppstd": "20",
        "compiler.libcxx": "libstdc++11",
        "os": "Linux",
        "arch": "x86_64",
    }


def profile_matches_platform(content, settings):
    """Check if an existing profile was generated for the same platform/compiler."""
    compiler = settings["compiler"]
    os_name = settings["os"]
    return f"compiler={compiler}" in content and f"os={os_name}" in content


def update_profile_settings(content, settings):
    """Update profile content with the target settings, preserving unknown fields."""
    for key, value in settings.items():
        pattern = rf"^{re.escape(key)}=.*$"
        replacement = f"{key}={value}"
        if re.search(pattern, content, re.MULTILINE):
            content = re.sub(pattern, replacement, content, flags=re.MULTILINE)
        else:
            content += f"\n{key}={value}"
    return content


def prepare_conan_profile():
    """Ensure a project-local Conan profile exists and is valid for the current platform."""
    profile_dir = Path(".conan/profiles")
    profile_path = profile_dir / "default"

    settings = get_default_platform_profile()

    if profile_path.exists():
        content = profile_path.read_text(encoding="utf-8")
        if profile_matches_platform(content, settings):
            old_version = None
            match = re.search(r"compiler\.version=(\S+)", content)
            if match:
                old_version = match.group(1)

            supported = get_supported_compiler_versions(settings["compiler"])
            current_version = old_version or settings["compiler.version"]
            if supported and current_version not in supported:
                settings["compiler.version"] = pick_nearest_version(
                    supported, current_version
                )

            new_content = update_profile_settings(content, settings)
            if new_content != content:
                profile_path.write_text(new_content, encoding="utf-8")
                if old_version and settings["compiler.version"] != old_version:
                    print(
                        f"[Build] Updated local Conan profile: {old_version} -> {settings['compiler.version']}"
                    )
                else:
                    print(f"[Build] Updated local Conan profile")
            return str(profile_path)

    # Generate new profile
    profile_dir.mkdir(parents=True, exist_ok=True)
    lines = ["[settings]"]
    for key, value in settings.items():
        lines.append(f"{key}={value}")
    profile_content = "\n".join(lines) + "\n"
    profile_path.write_text(profile_content, encoding="utf-8")
    print(
        f"[Build] Generated local Conan profile ({settings['compiler']} {settings['compiler.version']})"
    )
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
    new_check = (
        "cmake_policy(SET CMP0091 NEW)\ncmake_policy(GET CMP0091 POLICY_CMP0091)"
    )

    if new_check in content:
        return  # Already patched

    if old_check not in content:
        print(
            "[Build] Warning: Conan toolchain format changed; CMP0091 patch not applied. "
            "If CMake < 3.26 fails, please update CMake."
        )
        return

    content = content.replace(old_check, new_check)
    toolchain_path.write_text(content, encoding="utf-8")
    print("[Build] Patched conan_toolchain.cmake for CMP0091 compatibility")


def copy_compile_commands_to_root():
    """Copy build_ninja/compile_commands.json to the project root so clangd and
    other LSP clients can discover it without extra configuration."""
    src = Path("build_ninja/compile_commands.json")
    dst = Path("compile_commands.json")
    if src.exists():
        shutil.copy2(src, dst)
        print("[Build] Copied compile_commands.json to project root")


def generate_ninja_compile_commands(build_type, shipping):
    """Create a parallel Ninja configuration solely to produce
    compile_commands.json for LSP clients. The main build still uses the
    Visual Studio generator so that .sln files are available for VS IDE."""
    print("[Build] Generating Ninja configuration for compile_commands.json...")
    # Wipe any stale CMake cache to avoid generator/platform mismatch when
    # switching between generator types.
    build_ninja_dir = Path("build_ninja")
    if build_ninja_dir.exists():
        print(f"[Build] Removing stale {build_ninja_dir}...")
        shutil.rmtree(build_ninja_dir, ignore_errors=True)
    if build_ninja_dir.exists():
        print(f"[Build] Error: could not remove {build_ninja_dir}")
        sys.exit(1)

    # The Conan toolchain forces CMAKE_GENERATOR_PLATFORM=x64 for the Visual
    # Studio generator, but Ninja does not accept a platform. Create a Ninja
    # toolchain copy with that line stripped.
    ninja_toolchain = Path("build/conan/conan_toolchain_ninja.cmake")
    original_toolchain = Path("build/conan/conan_toolchain.cmake")
    if original_toolchain.exists():
        content = original_toolchain.read_text(encoding="utf-8")
        content = content.replace(
            'set(CMAKE_GENERATOR_PLATFORM "x64" CACHE STRING "" FORCE)\n', ""
        )
        content = content.replace(
            'set(CMAKE_GENERATOR_TOOLSET "v143" CACHE STRING "" FORCE)\n', ""
        )
        ninja_toolchain.write_text(content, encoding="utf-8")
    else:
        ninja_toolchain = original_toolchain

    ninja_cmd = [
        "cmake",
        "-S",
        ".",
        "-B",
        "build_ninja",
        "-G",
        "Ninja",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DCMAKE_TOOLCHAIN_FILE={ninja_toolchain}",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]
    if shipping:
        ninja_cmd += ["-DSHIPPING_BUILD=ON"]

    if sys.platform == "win32":
        # Ninja needs the MSVC environment; drive cmake through a VS developer
        # prompt so cl.exe/link.exe are available.
        vcvars = (
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\"
            "VC\\Auxiliary\\Build\\vcvarsall.bat"
        )
        if not Path(vcvars).exists():
            vcvars = (
                "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\"
                "VC\\Auxiliary\\Build\\vcvarsall.bat"
            )
        if not Path(vcvars).exists():
            print(
                "[Build] Warning: Could not find vcvarsall.bat; "
                "Ninja configuration may fail without MSVC environment."
            )
            run(ninja_cmd)
        else:
            # Write a temporary batch file to avoid quoting/escaping issues
            # when passing vcvarsall + cmake through cmd /C.
            batch = Path("build_ninja_setup.bat")
            batch.write_text(
                f"@echo off\n"
                f'call "{vcvars}" amd64\n'
                f"{' '.join(str(c) for c in ninja_cmd)}\n",
                encoding="utf-8",
            )
            try:
                run(["cmd", "/C", str(batch)])
            finally:
                batch.unlink(missing_ok=True)
    else:
        run(ninja_cmd)
    copy_compile_commands_to_root()


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
            {"kind": "toolchains", "version": 1},
        ]
    }
    query_file.write_text(json.dumps(query), encoding="utf-8")
    print("[Build] Prepared CMake File API query for IDE IntelliSense")


def main():
    parser = argparse.ArgumentParser(description="Entelechy Build Script")
    parser.add_argument(
        "--config", default="configs/full_build.json", help="Path to build config JSON"
    )
    parser.add_argument(
        "--debug", action="store_true", help="Build Debug configuration"
    )
    parser.add_argument(
        "--release", action="store_true", help="Build Release configuration"
    )
    parser.add_argument(
        "--build", action="store_true", help="Also compile after CMake configuration"
    )
    parser.add_argument(
        "--shipping",
        action="store_true",
        help="Shipping build (strips debug/info logs)",
    )
    parser.add_argument(
        "--strict-build",
        action="store_true",
        help="Force building all dependencies from source "
        "(uses --build=*) to avoid Conan MSVC "
        "compatibility fallback issues",
    )
    args = parser.parse_args()

    build_type = "Debug" if args.debug else "Release"
    if args.release:
        build_type = "Release"

    config_path = args.config
    print(f"[Build] Config: {config_path}")
    print(f"[Build] Type: {build_type}")

    # Prepare project-local Conan profile (adapts to current Conan version)
    profile_path = prepare_conan_profile()

    # Step 0: Install dependencies via Conan
    conan_executable = get_conan_cmd()
    if conan_executable == "conan" and shutil.which("conan") is None:
        print(
            "[Build] Error: 'conan' not found. "
            "Please run: python scripts/tools/setup_env.py"
        )
        sys.exit(1)

    print("[Build] Step 0: Installing dependencies via Conan...")
    print(f"[Build] Using conan: {conan_executable}")

    def run_conan_install(bt):
        cmd = [
            conan_executable,
            "install",
            ".",
            "--output-folder=build/conan",
            f"--profile:host={profile_path}",
            f"--profile:build={profile_path}",
            "-s",
            f"build_type={bt}",
        ]
        if args.strict_build:
            # Force building host dependencies from source so they match the
            # exact compiler.version (avoiding MSVC 194 -> 193 compatibility
            # fallback). Build requirements (e.g. cmake) still use
            # --build=missing so their prebuilt binaries can be downloaded/built
            # normally.
            cmd += [
                "--build=missing",
                "--build=entelechy/*",
                "--build=glad/*",
                "--build=glfw/*",
                "--build=imgui/*",
                "--build=mimalloc/*",
                "--build=opengl/*",
            ]
            print(
                "[Build] Strict build enabled: host dependencies will be built from source"
            )
        else:
            cmd += ["--build=missing"]
        run(cmd)

    run_conan_install(build_type)
    # Visual Studio Multi-Config needs both Debug and Release package info to
    # avoid IMPORTED_LOCATION errors during CMake configuration.
    if sys.platform == "win32" and build_type == "Debug":
        run_conan_install("Release")
    elif sys.platform == "win32" and build_type == "Release":
        run_conan_install("Debug")

    # Patch toolchain for old CMake compatibility (no-op on CMake >= 3.26)
    patch_conan_toolchain()

    # Step 0: Ensure shared agent skills are linked under .claude/skills so
    # that Claude Code and other agents see the same skills directory.
    # Idempotent — no-op if the link already points at the right target.
    project_root = Path(__file__).resolve().parents[2]
    setup_skills_link(project_root)

    # Step 1: Source Forest generation is now handled by CMake configure_file.
    # (legacy generator.py removed — modules are discovered directly by CMake.)

    # Prepare CMake File API query for IDE IntelliSense (Visual Studio generator
    # does not generate compile_commands.json, so we rely on File API replies)
    prepare_cmake_file_api()

    # Step 2: Run CMake (Visual Studio generator keeps .sln for VS IDE)
    print("[Build] Step 2: Running CMake...")
    cmake_cmd = ["cmake", "-S", ".", "-B", "build"]
    if sys.platform == "win32":
        cmake_cmd += [
            "-G",
            "Visual Studio 17 2022",
            "-A",
            "x64",
            "-DCMAKE_CONFIGURATION_TYPES=Debug;Release",
        ]
    else:
        cmake_cmd += [f"-DCMAKE_BUILD_TYPE={build_type}"]
    cmake_cmd += ["-DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake"]
    cmake_cmd += ["-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"]
    if args.shipping:
        cmake_cmd += ["-DSHIPPING_BUILD=ON"]
    run(cmake_cmd)

    # Step 2b: Generate a parallel Ninja configuration to produce
    # compile_commands.json for LSP clients (Zed, clangd, etc.)
    generate_ninja_compile_commands(build_type, args.shipping)

    # Step 3: Build (optional)
    if args.build:
        print(f"[Build] Step 3: Building {build_type}...")
        run(["cmake", "--build", "build", "--config", build_type, "--parallel"])
        print("[Build] Success! Output: build/bin/")
    else:
        print("[Build] Done. (Use --build to also compile)")


if __name__ == "__main__":
    main()
