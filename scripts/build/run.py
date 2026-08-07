#!/usr/bin/env python3
"""
Entelechy One-Command Run Script
Usage: python scripts/build/run.py [ --debug | --release ] [ --force-cook ] [ --no-cook ]

Runs the asset cooking step (MeshCooker, only when inputs changed) and then
launches the engine executable. Shader compilation is already handled by the
CMake POST_BUILD step, so a normal workflow is:

    python scripts/build/build.py --debug --compile-only   # build (+shaders)
    python scripts/build/run.py --debug                    # cook (if stale) + run
"""

import argparse
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]

SPONZA_DIR = PROJECT_ROOT / "_content" / "sponza"
COOK_INPUTS = [
    SPONZA_DIR / "NewSponza_Main_glTF_003.gltf",
    SPONZA_DIR / "NewSponza_Main_glTF_003.bin",
]
COOK_OUTPUT_DIR = SPONZA_DIR / "cooked"
# scene.json is the last file MeshCooker writes; use it as the cook marker.
COOK_MARKER = COOK_OUTPUT_DIR / "scene.json"


def run(cmd, cwd=None):
    """Run a command and forward stdout/stderr. Exit on failure."""
    print(f"[Run] {' '.join(str(c) for c in cmd)}")
    result = subprocess.run([str(c) for c in cmd], cwd=cwd)
    if result.returncode != 0:
        sys.exit(result.returncode)


def cook_is_stale(cooker_exe):
    """Return True if cooked assets are missing or older than their inputs."""
    if not COOK_MARKER.exists():
        return True
    marker_mtime = COOK_MARKER.stat().st_mtime
    for source in [*COOK_INPUTS, cooker_exe]:
        if source.exists() and source.stat().st_mtime > marker_mtime:
            return True
    return False


def main():
    parser = argparse.ArgumentParser(description="Entelechy Run Script")
    parser.add_argument("--debug", action="store_true", help="Run Debug build (default)")
    parser.add_argument("--release", action="store_true", help="Run Release build")
    parser.add_argument(
        "--force-cook",
        action="store_true",
        help="Re-cook assets even if outputs look up-to-date",
    )
    parser.add_argument(
        "--no-cook", action="store_true", help="Skip the asset cooking step"
    )
    args = parser.parse_args()

    build_type = "Release" if args.release else "Debug"
    bin_dir = PROJECT_ROOT / "build" / "bin" / build_type
    engine_exe = bin_dir / "Entelechy.exe"
    cooker_exe = bin_dir / "MeshCooker.exe"

    if not engine_exe.exists():
        print(
            f"[Run] Error: {engine_exe} not found. "
            f"Build first: python scripts/build/build.py --{build_type.lower()} --build"
        )
        sys.exit(1)

    # Step 1: Cook assets if stale (CMake's CookAssets target already does this
    # on build; this is a safety net for runs without a preceding build).
    if not args.no_cook:
        if not cooker_exe.exists():
            print(f"[Run] Error: {cooker_exe} not found. Build first.")
            sys.exit(1)
        if args.force_cook or cook_is_stale(cooker_exe):
            print("[Run] Step 1: Cooking assets (MeshCooker)...")
            run(
                [
                    cooker_exe,
                    COOK_INPUTS[0],
                    COOK_OUTPUT_DIR,
                ],
                cwd=PROJECT_ROOT,
            )
        else:
            print("[Run] Step 1: Cooked assets are up-to-date, skipping cook.")

    # Step 2: Launch the engine from the project root so that relative content
    # paths (_content/...) resolve the same way as in the tools.
    print(f"[Run] Step 2: Launching {engine_exe}...")
    run([engine_exe], cwd=PROJECT_ROOT)


if __name__ == "__main__":
    main()
