#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${1:-0.3.0}"
BUNDLE="${2:-$ROOT/build-fusion/ToonPixel.plugin}"
OUT_DIR="$ROOT/artifacts/installer"
PACKAGE="$OUT_DIR/ToonPixel-$VERSION-dev.pkg"

codesign --verify --deep --strict "$BUNDLE"
lipo -info "$BUNDLE/Contents/MacOS/ToonPixel"
plutil -lint "$BUNDLE/Contents/Info.plist"
mkdir -p "$OUT_DIR"
WORK_DIR="$(mktemp -d "$OUT_DIR/.candidate.XXXXXX")"
CANDIDATE="$WORK_DIR/ToonPixel-$VERSION-dev.pkg"
EXPAND_DIR="$WORK_DIR/expanded"
SUCCESS=0
trap 'if [[ "$SUCCESS" -eq 1 ]]; then rm -rf "$WORK_DIR"; else printf "candidate_retained=%s\n" "$WORK_DIR" >&2; fi' EXIT

pkgbuild \
    --component "$BUNDLE" \
    --install-location "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/ToonPixel/ToonPixel.plugin" \
    --identifier com.toonpixel.aftereffects.toonpixel \
    --version "$VERSION" \
    --ownership recommended \
    "$CANDIDATE"

set +e
SIGNATURE="$(pkgutil --check-signature "$CANDIDATE" 2>&1)"
SIGNATURE_STATUS=$?
set -e
if [[ $SIGNATURE_STATUS -ne 0 && "$SIGNATURE" != *"no signature"* ]]; then
    printf '%s\n' "$SIGNATURE" >&2
    exit "$SIGNATURE_STATUS"
fi
printf '%s\n' "$SIGNATURE" > "$OUT_DIR/signature.txt"
pkgutil --payload-files "$CANDIDATE" > "$OUT_DIR/payload-files.txt"
pkgutil --expand-full "$CANDIDATE" "$EXPAND_DIR"
diff -rq "$BUNDLE" "$EXPAND_DIR/Payload/ToonPixel.plugin"
codesign --verify --deep --strict "$EXPAND_DIR/Payload/ToonPixel.plugin"

if [[ -f "$PACKAGE" ]]; then
    mkdir -p "$OUT_DIR/backups"
    cp -p "$PACKAGE" "$OUT_DIR/backups/ToonPixel-$VERSION-dev-$(date +%Y%m%d-%H%M%S).pkg"
fi
mv "$CANDIDATE" "$PACKAGE"
shasum -a 256 "$PACKAGE" > "$OUT_DIR/SHA256SUMS"
if [[ "$SIGNATURE_STATUS" -eq 0 ]]; then
    PACKAGE_SIGNING="signed"
else
    PACKAGE_SIGNING="unsigned"
fi
printf 'version=%s\npackage=%s\nsignature_status=%s\npackage_signing=%s\n' \
    "$VERSION" "$PACKAGE" "$SIGNATURE_STATUS" "$PACKAGE_SIGNING" > "$OUT_DIR/build-info.txt"
SUCCESS=1
printf 'PACKAGE=%s\n' "$PACKAGE"
