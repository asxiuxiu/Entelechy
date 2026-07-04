#!/usr/bin/env python3
"""
Project environment configuration loader.

Reads the base configuration from `configs/environment.json` and merges any
machine-specific overrides from `configs/environment.local.json`.
"""

import json
import platform
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
BASE_CONFIG_PATH = PROJECT_ROOT / "configs" / "environment.json"
LOCAL_CONFIG_PATH = PROJECT_ROOT / "configs" / "environment.local.json"


def _deep_merge(base, override):
    """Recursively merge override into base. Lists are concatenated."""
    if isinstance(base, dict) and isinstance(override, dict):
        result = dict(base)
        for key, value in override.items():
            if key in result:
                result[key] = _deep_merge(result[key], value)
            else:
                result[key] = value
        return result
    if isinstance(base, list) and isinstance(override, list):
        return base + override
    return override


def _parse_version(version_str):
    """Parse a version string like '3.12.0' into a tuple of ints."""
    parts = version_str.split(".")
    parsed = []
    for part in parts:
        try:
            parsed.append(int(part))
        except ValueError:
            break
    return tuple(parsed) if parsed else (0,)


def _check_python_version(min_version_str):
    """Fail with a clear message if the running Python is too old."""
    min_version = _parse_version(min_version_str)
    current = sys.version_info[: len(min_version)]
    if current < min_version:
        print(
            f"[EnvConfig] Error: Python {min_version_str}+ required, "
            f"but running {platform.python_version()}."
        )
        sys.exit(1)


def load_env_config():
    """Load and return the merged project environment configuration."""
    if not BASE_CONFIG_PATH.exists():
        print(f"[EnvConfig] Error: base config not found: {BASE_CONFIG_PATH}")
        sys.exit(1)

    config = json.loads(BASE_CONFIG_PATH.read_text(encoding="utf-8"))

    if LOCAL_CONFIG_PATH.exists():
        local_config = json.loads(LOCAL_CONFIG_PATH.read_text(encoding="utf-8"))
        config = _deep_merge(config, local_config)

    python_min = config.get("python", {}).get("min_version")
    if python_min:
        _check_python_version(python_min)

    return config


def get_project_root():
    return PROJECT_ROOT


if __name__ == "__main__":
    cfg = load_env_config()
    print(json.dumps(cfg, indent=4))
