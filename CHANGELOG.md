# Changelog

## Unreleased

## 1.0.0-beta.8 (display 1.0.4)

- Time zone is stored on the board: pick it in the web UI (Settings → Time zone); the browser computes the offset and DST rules and the board applies them, so the display clock and workout records use local time. Default until set: Asia/Yekaterinburg.
- Display: main page status bar (time, heart rate, heart icon) refreshes every 250 ms, so values appear right after opening the page.
- Display info page: ESPHome version shown without the config hash.
- Public build: Bluetooth is switched off during an upload from the ESPHome Dashboard (web server OTA), as in the private build; fewer stalled uploads (HTTP 408).
- Display TFT 1.0.4: new main and settings page design; Bluetooth list rows hidden on open.

## 1.0.0-beta.7 (display 1.0.3)

Firmware 1.0.0-beta.7 and display TFT 1.0.3 must be updated together: the main page button ids changed.

- Display: main page buttons renumbered for the new TFT (Quick start 3, HR Zone 4, HIIT 5, Map 6, Settings 7, Fartlek 8); Quick start from the display works again. The Fartlek tile is now `bfartlek`. Old page 11 (mode select) bindings removed.
- Web UI: the last known values are kept in the browser and shown at once on the next visit; header badges, motor temperature ("—°" until known) and the zone bar keep their place while live data arrives, so the page no longer jumps.
- Leak watch: the board tracks idle free internal RAM against a baseline taken after 1 h of uptime (history in PSRAM) and publishes "Memory Trend" (collecting / stable / decreasing) and "Memory Drop" (KB); the web UI Memory tile shows "stable · <uptime>" or "decreasing: −N KB", plus free PSRAM.
- Web UI diagnostics: "low memory" only when free memory stays low for over a minute (short dips during update checks or page loads are normal); "Firmware loop" and "Page" tiles removed from the summary (still in the copied report).
- Display: heart-rate monitor can be searched for and selected on the Nextion Bluetooth page (19): Search, 4 result rows (connected monitor first), status and current address. Bluetooth page fields renamed to tHr / tModel / tBatt; requires the matching TFT.
- Display: main page status bar shows time (`mainPage.tTime`), heart rate (`mainPage.tHr`) and heart icon state (`mainPage.pHr`); requires the matching TFT.
- Web UI: the "Form" card in Progress now explains what is missing (best matching speed/incline count out of 4) instead of showing the total workout count.
