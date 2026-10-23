#!/bin/sh
# Usage: fw-release.sh <version> <path/to/firmware.ota.bin> [summary]
# Publishes firmware release fw-v<version> and refreshes the fixed "fw-manifest" release that the device polls.
# Needs: gh (logged in), md5sum. Add --prerelease via PRE=1.
set -e
V="$1"; BIN="$2"; SUM="${3:-Treadmill firmware $1}"
[ -n "$V" ] && [ -f "$BIN" ] || { echo "usage: $0 <version> <firmware.ota.bin> [summary]"; exit 1; }
REPO="samsonovss/ESPHome-Treadmill-FTMS"
TAG="fw-v$V"; NAME="treadmill-fw-$V.ota.bin"
TMP=$(mktemp -d); cp "$BIN" "$TMP/$NAME"
MD5=$(md5sum "$TMP/$NAME" | cut -d' ' -f1)
cat > "$TMP/manifest.json" <<J
{"name":"Treadmill","version":"$V","builds":[{"chipFamily":"ESP32-S3","ota":{"path":"https://github.com/$REPO/releases/download/$TAG/$NAME","md5":"$MD5","summary":"$SUM","release_url":"https://github.com/$REPO/releases/tag/$TAG"}}]}
J
NOTES="$SUM

## First install: USB only
The first flash must be done over **USB** (ESPHome Web / \`esphome run\` with the factory image), not OTA: the partition layout differs from older builds. After that one USB flash, later updates can be done over Wi-Fi from the device web page (one click).

Первая прошивка - **только по USB** (разметка разделов отличается от старых версий). Дальше обновления идут по Wi-Fi одной кнопкой на веб-странице."
gh release create "$TAG" "$TMP/$NAME" --repo "$REPO" --title "Firmware v$V" --notes "$NOTES" ${PRE:+--prerelease}
gh release view fw-manifest --repo "$REPO" >/dev/null 2>&1 || gh release create fw-manifest --repo "$REPO" --title "Firmware manifest" --notes "Auto-updated manifest polled by devices. Do not delete." --prerelease
gh release upload fw-manifest "$TMP/manifest.json" --repo "$REPO" --clobber
echo "Done: $TAG md5=$MD5"; rm -rf "$TMP"
