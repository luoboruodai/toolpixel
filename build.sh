#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
cmake -S . -B build-fusion -DTOONPIXEL_DEBUG_LOG=OFF
cmake --build build-fusion --parallel 2
ctest --test-dir build-fusion --output-on-failure
bash tools/build_dev_package.sh "${1:-0.1.2}" build-fusion/ToonPixel.plugin
