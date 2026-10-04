#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ $# -gt 1 || ( $# -eq 1 && "$1" != --verify ) ]]; then
    echo "Usage: tools/build_windows.sh [--verify]" >&2
    exit 2
fi
python3 tools/setup_windows.py
cmake --preset windows-mingw64
cmake --build --preset windows-mingw64 --parallel "${AYPLAYER_BUILD_JOBS:-8}"
python3 tools/package_windows.py
if [[ "${1:-}" == --verify ]]; then
    python3 tools/verify_windows.py
    python3 tools/package_windows.py
fi
