#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

command -v pio >/dev/null || { echo "PlatformIO (pio) is required" >&2; exit 2; }

echo "== Native regression tests =="
scripts/test-native.sh

echo "== Core2 firmware build =="
pio run -e m5stack-core2
