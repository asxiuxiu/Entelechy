#!/usr/bin/env python3
"""
Ensure the prebuilt DirectX Shader Compiler (DXC) binaries exist under
third_party/dxc/. The headers are committed to git, but bin/ and lib/ are
gitignored, so they are downloaded from the official GitHub release on demand.

The release version/asset is pinned in configs/environment.json ("dxc" section).
Optional download mirrors (URL prefixes, e.g. a GitHub proxy) can be added via
configs/environment.local.json:

    "dxc": { "url_prefixes": ["https://ghfast.top/"] }

Usage:
    python scripts/tools/ensure_dxc.py [--force]
"""

import argparse
import os
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

# Allow importing env_config from the same directory.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from env_config import get_project_root, load_env_config

PROJECT_ROOT = get_project_root()
DXC_DIR = PROJECT_ROOT / "third_party" / "dxc"
VERSION_MARKER = DXC_DIR / "bin" / ".version"

REQUIRED_FILES = [
    DXC_DIR / "bin" / "x64" / "dxc.exe",
    DXC_DIR / "bin" / "x64" / "dxcompiler.dll",
    DXC_DIR / "bin" / "x64" / "dxil.dll",
    DXC_DIR / "lib" / "x64" / "dxcompiler.lib",
]

RELEASE_URL_TEMPLATE = (
    "https://github.com/microsoft/DirectXShaderCompiler/"
    "releases/download/{version}/{asset}"
)


def dxc_is_present(version):
    """Check that all required binaries exist and match the pinned version."""
    if not all(f.exists() for f in REQUIRED_FILES):
        return False
    if not VERSION_MARKER.exists():
        return False
    return VERSION_MARKER.read_text(encoding="utf-8").strip() == version


def _download(url, dest):
    """Download url to dest, printing simple progress. Raises on failure."""
    print(f"[DXC] Downloading: {url}")
    with urllib.request.urlopen(url, timeout=60) as response:
        total = int(response.headers.get("Content-Length") or 0)
        received = 0
        next_report = 0
        with open(dest, "wb") as out:
            while True:
                chunk = response.read(1 << 20)
                if not chunk:
                    break
                out.write(chunk)
                received += len(chunk)
                if received >= next_report:
                    mb = received / (1024 * 1024)
                    if total:
                        print(f"[DXC]   {mb:.1f} / {total / (1024 * 1024):.1f} MB")
                    else:
                        print(f"[DXC]   {mb:.1f} MB")
                    next_report = received + (10 << 20)


def download_dxc_zip(dxc_cfg):
    """Download the pinned DXC release zip, trying mirror prefixes first."""
    version = dxc_cfg["version"]
    asset = dxc_cfg["asset"]
    direct_url = RELEASE_URL_TEMPLATE.format(version=version, asset=asset)

    urls = [f"{prefix}{direct_url}" for prefix in dxc_cfg.get("url_prefixes", [])]
    urls.append(direct_url)

    fd, tmp_path = tempfile.mkstemp(suffix=".zip", prefix="entelechy_dxc_")
    os.close(fd)
    try:
        last_error = None
        for url in urls:
            try:
                _download(url, tmp_path)
                return Path(tmp_path)
            except Exception as e:
                print(f"[DXC] Download failed via this mirror: {e}")
                last_error = e
        print(f"[DXC] Error: all download sources failed. Last error: {last_error}")
        sys.exit(1)
    except BaseException:
        Path(tmp_path).unlink(missing_ok=True)
        raise


def extract_dxc_zip(zip_path):
    """Extract bin/x64 and lib/x64 from the DXC release zip into third_party/dxc.

    The official zip uses backslash path separators in entry names, so names
    are normalized before matching.
    """
    print(f"[DXC] Extracting to {DXC_DIR} ...")
    count = 0
    with zipfile.ZipFile(zip_path) as z:
        for name in z.namelist():
            parts = name.replace("\\", "/").split("/")
            if len(parts) == 3 and parts[0] in ("bin", "lib") and parts[1] == "x64":
                dest = DXC_DIR / parts[0] / "x64" / parts[2]
                dest.parent.mkdir(parents=True, exist_ok=True)
                with z.open(name) as src, open(dest, "wb") as dst:
                    dst.write(src.read())
                count += 1
    if count == 0:
        print("[DXC] Error: no bin/x64 or lib/x64 entries found in the zip.")
        sys.exit(1)

    for required in REQUIRED_FILES:
        if not required.exists():
            print(f"[DXC] Error: expected file missing after extraction: {required}")
            sys.exit(1)


def ensure_dxc(config, force=False):
    """Ensure DXC binaries are present; download and extract them if not."""
    dxc_cfg = config.get("dxc")
    if not dxc_cfg:
        print("[DXC] No 'dxc' section in environment config, skipping.")
        return

    if sys.platform != "win32":
        print("[DXC] Non-Windows host: prebuilt DXC layout is Windows-only, skipping.")
        return

    version = dxc_cfg["version"]
    if not force and dxc_is_present(version):
        print(f"[DXC] {version} already present, skipping.")
        return

    zip_path = download_dxc_zip(dxc_cfg)
    try:
        extract_dxc_zip(zip_path)
    finally:
        zip_path.unlink(missing_ok=True)

    VERSION_MARKER.parent.mkdir(parents=True, exist_ok=True)
    VERSION_MARKER.write_text(version + "\n", encoding="utf-8")
    print(f"[DXC] {version} ready in {DXC_DIR}")


def main():
    parser = argparse.ArgumentParser(
        description="Download prebuilt DXC binaries into third_party/dxc/."
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Re-download even if the pinned version is already present.",
    )
    args = parser.parse_args()
    ensure_dxc(load_env_config(), force=args.force)


if __name__ == "__main__":
    main()
