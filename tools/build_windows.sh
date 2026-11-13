#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -n "${AE_SDK_ROOT:-}" ]]; then
  SDK="$AE_SDK_ROOT"
elif [[ -d "$ROOT/vendor/ae-sdk/Headers" ]]; then
  SDK="$ROOT/vendor/ae-sdk"
else
  SDK="$ROOT/SDK"
fi
CXX="${MINGW_CXX:-x86_64-w64-mingw32-g++}"
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
STRIP="${MINGW_STRIP:-x86_64-w64-mingw32-strip}"
OUT="${TOONPIXEL_WIN_OUT:-$ROOT/build-win}"

[[ -d "$SDK/Headers" ]] || { printf 'Missing Adobe AE SDK headers: %s\n' "$SDK" >&2; exit 2; }
for tool in "$CXX" "$WINDRES" "$STRIP"; do
  command -v "$tool" >/dev/null 2>&1 || [[ -x "$tool" ]] || { printf 'Missing Windows tool: %s\n' "$tool" >&2; exit 2; }
done
mkdir -p "$OUT"
python3 "$ROOT/tools/win/generate_pipl.py" "$OUT/ToonPixel.pipl"
cp "$ROOT/tools/win/ToonPixelWin.rc" "$OUT/ToonPixel.rc"
(
  cd "$OUT"
  "$WINDRES" -O coff -i ToonPixel.rc -o ToonPixel.pipl.res
)
python3 "$ROOT/tools/win/preserve_pipl_case.py" "$OUT/ToonPixel.pipl.res"

"$CXX" -shared -static -O2 -std=c++17 \
  -D_WIN32 -DMSWindows -D_WINDOWS -DTOONPIXEL_HOST=0 \
  -Wall -Wextra -Wno-unused-parameter -Wno-register \
  -I"$ROOT/src/core" -I"$ROOT/src/ae" \
  -I"$SDK/Headers" -I"$SDK/Headers/Win" -I"$SDK/Headers/SP" \
  -I"$SDK" -I"$SDK/Util" \
  "$ROOT/src/ae/ToonPixel.cpp" "$OUT/ToonPixel.pipl.res" \
  "$ROOT/tools/win/ToonPixelWin.def" \
  -Wl,--subsystem,windows -o "$OUT/ToonPixel.aex"
"$STRIP" --strip-unneeded "$OUT/ToonPixel.aex"
file "$OUT/ToonPixel.aex"
python3 "$ROOT/tools/check_windows_binary.py" "$OUT/ToonPixel.aex"
printf 'built: %s\n' "$OUT/ToonPixel.aex"
