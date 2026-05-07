#!/usr/bin/env bash
# Entelechy Build Wrapper (macOS / Linux)
# Forwards all arguments to build.py
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "${SCRIPT_DIR}/build.py" "$@"
