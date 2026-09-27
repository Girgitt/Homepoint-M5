#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

command -v pio >/dev/null || {
  echo "PlatformIO (pio) is required" >&2
  exit 2
}

pio test -e native
