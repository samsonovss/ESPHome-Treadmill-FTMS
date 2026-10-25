#!/bin/sh
# Usage: fw-release.sh <version> <firmware.ota.bin> <firmware.factory.bin> [summary]
# Both files come from the ESPHome dashboard: treadmill-public.yaml -> Install -> Manual download (OTA / Factory format).
# Publishes firmware release fw-v<version> and refreshes the fixed "fw-manifest" release that the device polls.
# Needs: gh (logged in), md5sum. Add --prerelease via PRE=1.
set -e
V="$1"; BIN="$2"; FACTORY="$3"; SUM="${4:-Treadmill firmware $1}"
[ -n "$V" ] && [ -f "$BIN" ] && [ -f "$FACTORY" ] || { echo "usage: $0 <version> <firmware.ota.bin> <firmware.factory.bin> [summary]"; exit 1; }
REPO="samsonovss/ESPHome-Treadmill-FTMS"
TAG="fw-v$V"; NAME="treadmill-fw-$V.ota.bin"
FNAME="treadmill-fw-$V.factory.bin"
TMP=$(mktemp -d); cp "$BIN" "$TMP/$NAME"; cp "$FACTORY" "$TMP/$FNAME"
MD5=$(md5sum "$TMP/$NAME" | cut -d' ' -f1)
cat > "$TMP/manifest.json" <<J
{"name":"Treadmill","version":"$V","builds":[{"chipFamily":"ESP32-S3","ota":{"path":"https://github.com/$REPO/releases/download/$TAG/$NAME","md5":"$MD5","summary":"$SUM","release_url":"https://github.com/$REPO/releases/tag/$TAG"}}]}
J
NOTES="$SUM

## First install: USB only
Flash \`$FNAME\` over **USB** from the [install page](https://samsonovss.github.io/ESPHome-Treadmill-FTMS/install/) (Chrome/Edge) or [ESPHome Web](https://web.esphome.io). After that one USB flash, updates come over Wi-Fi from the treadmill's web page (\`$NAME\`, one click).

Первая прошивка - **только по USB**: файл \`$FNAME\` со [страницы установки](https://samsonovss.github.io/ESPHome-Treadmill-FTMS/install/) (Chrome/Edge) или через [ESPHome Web](https://web.esphome.io). Дальше обновления идут по Wi-Fi одной кнопкой на веб-странице дорожки."
gh release create "$TAG" "$TMP/$NAME" "$TMP/$FNAME" --repo "$REPO" --title "Firmware v$V" --notes "$NOTES" ${PRE:+--prerelease}
gh release view fw-manifest --repo "$REPO" >/dev/null 2>&1 || gh release create fw-manifest --repo "$REPO" --title "Firmware manifest" --notes "Auto-updated manifest polled by devices. Do not delete." --prerelease
gh release upload fw-manifest "$TMP/manifest.json" --repo "$REPO" --clobber
gh workflow run web-ui.yml --repo "$REPO" --ref main  # refresh the USB install page
echo "Done: $TAG md5=$MD5"; rm -rf "$TMP"
