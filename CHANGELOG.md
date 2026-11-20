# Changelog

## Unreleased

## 1.0.0-beta.15 (display 1.0.5)

- Enable one-click GitHub updates for no-display builds via a separate OTA manifest. Existing Nextion builds keep their original update feed.

## 1.0.0-beta.14 (display 1.0.5)

- Fix FTMS discovery after power-on with ESPHome 2026.11.0. The local BLE server now explicitly requests advertising when it starts, so discovery does not depend on connecting a heart-rate monitor first.

## 1.0.0-beta.13 (display 1.0.5)

- Make the Nextion display optional at build time (`display_mode: "1"` for Nextion, `"0"` without a display). The no-display build omits the Nextion driver and its UART; the web UI and FTMS remain available. Mode `"2"` is reserved for LVGL.
- Offer both builds in the USB web installer. The built-in GitHub OTA feed still serves Nextion only.

## 1.0.0-beta.12 (display 1.0.5)

- Use the built-in Nextion driver in ESPHome 2026.11.0 instead of the local override. Keep `tft_upload_http_buffer_size_tx: 1024` for GitHub TFT redirects (ESPHome #21435) and retain the separate BLE override for FTMS/Zwift.

- Switch from the external VL53L1X driver to the built-in component in ESPHome 2026.11.0. Preserve millimetres for `My Raw Distance` and centimetres for `My Distance`. Verified on the treadmill: Nextion showed 50 cm at a measured distance of 50 cm, and RunFree worked in a separate hands-on test.

- Add a maintenance card with an on-device odometer shared by all profiles and a configurable belt lubrication reminder.

- Preserve diagnostics text selection across periodic updates and new journal events; retain stable event rows and defer changes to selected text until selection collapses.

- Fix empty HIIT/fartlek forecasts and restore workout summaries and heart-rate zones when returning to History. Improve contrast in the fitness chart and open the annual consistency calendar at the latest weeks.

## 1.0.0-beta.11 (display 1.0.5)

- Align motor-board service values in fixed input/unit columns; keep acknowledgements below the fields.
- Integrate approved variant E header: shared compact status/profile/clock/language enclosure, cohesive icons and separators, retaining responsive navigation, profile menu behavior and motor-temperature warnings.

## 1.0.0-beta.10 (display 1.0.5)

- Recorder bulk-summary pages return at most two records (requested limit remains an upper bound), preserving the cursor and summary data. Live read-only checks found 13,213-byte responses stalled after 1,327/2,767 bytes; one/two-record responses completed in 0.17/0.31 seconds. This is a bounded-payload mitigation, not a proven TCP/root-cause fix; device verification remains separate.

- History startup waits for an applied profile, independently of control freshness/lock state. Partial refreshes retain known cards and metadata until complete reconciliation; unchanged-list cache metadata is no longer cleared prematurely.

- History detail stays coherent after user switches: cached list publication is awaited, opened records retain their summary/zone snapshot, and delayed personal route loading hydrates the already-open map/title without another click. Summary rendering keys include profile epoch and actual metadata/zone values.

- Nextion HIIT/fartlek pages retain five-row pagination and show personal presets followed by `Стандарт · …`; all six standards are generated from the canonical web definitions, never seeded into personal files. Selection also applies stored warm-up/cool-down modes and rejects stale kind/revision/profile snapshots.
- History requests opt-in descending pagination: recent matching cards and graph load before older pages. Unchanged archives use a cheap boot/latest/revision status token instead of rescanning sparse IDs every 15 seconds; legacy assignment and complete Progress pagination remain available. Older firmware falls back to ascending pagination without auto-opening an oldest partial record.

- Recorder bulk summaries now accept trailing JSON whitespace (including the newline written by recovered records), avoiding unnecessary per-record fallback requests.
- History loading: publish available summaries while paging, open the newest matching workout before unrelated summary reads, revalidate lists after profile changes/stop and periodically in History, discard stale responses, and retry busy/unavailable archives without caching errors as empty. Empty state requires completed pagination/metadata and a ready profile; bulk metadata replaces stale browser ownership hints.

- “My routes” (imports, custom routes and saved Zwift copies) are profile-owned on Web and Nextion; built-ins/public Zwift stay shared. Legacy board JSON/ACT files migrate together to the original default user with byte-preserving backups. Pinned-owner APIs and browser epochs reject stale reads/writes; favorites are personal, calibration/catalog/view preferences remain shared. Switching user while stopped clears the previous Custom route slot; running-profile lock is unchanged.

- Global persistent user profiles: selector, CRUD, pending/applied acknowledgment and boot/revision conflict checks. Profile editor separates name/age/sex/weight from HR settings, with confirmed activation before editing another user's zones.
- Profile UI: built-in sex-based SVG avatars, active-user badge, compact deduplicated header selector, localized age/weight text, equal-size Add card and stable keyed cards/menu preserving selection, hover and first-click editing during polling.
- Completed workouts freeze applied profile ID/name at start. History and Progress use the selected user's records; unknown/unassigned records are excluded from personal analysis. Legacy assignment offers all/selected records with name/count confirmation, atomic metadata-only writes, readback without replaying ambiguous POSTs and an 8-second success-message timer. Recovery without owner metadata remains unassigned.
- Saved pulse/HIIT/fartlek presets and MCP programs are personal; built-ins remain shared. Durable legacy migration binds to p1 (fallback: first persistent profile), preserves original-file backups and retries safely. Browser-only localStorage migration is **not implemented**; originals remain for recovery. Owner-checked writes and stale Web/Nextion selection guards prevent cross-profile updates.
- Nextion main page displays the applied user in `tUser`; saved HIIT/fartlek selections reload and filter by owner. Latest existing display bindings are retained.
- MCP adds applied profile context and explicit pinned-owner history/program reads and writes; pagination continues across empty filtered pages, unknown/unassigned history is excluded, and profile mismatches fail instead of silently switching users. HTTP workout reads gain opt-in owner filtering while legacy UI/assignment reads stay compatible. Live snapshots expose their recorded owner, not the newly selected user.
- Source publication accompanies the existing beta.10 binaries. Published release tags are not moved; see the release notes for the exact source commit. Intermittent `/api/profiles` HTTP 500 remains under investigation; full hardware regression and post-power-loss owner recovery are not claimed.

- Web UI diagnostics redesigned with five responsive cards: internal free RAM and largest block, free PSRAM, Wi-Fi signal, uptime/reset reason, and file storage (free/used/total with a usage bar). File storage is the data partition, not the entire flash chip.
- Diagnostics now includes a browser-session memory chart (free RAM and largest block, up to 1200 observations), recent browser-local events, expandable technical details, and report copying. No historical chart is downloaded from the board; connections lost, reboots, and stale measurements break chart lines. Cached values are not presented as fresh readings.
- Diagnostics summary distinguishes waiting for fresh data, disconnected state, decreasing memory, and sustained low memory. Existing full event messages and report details are retained.
- Diagnostics layout refined for desktop/tablet/mobile: five cards in one row when the panel is wide enough, consistent text sizes, compact spacing, aligned card sections, outline icons, and readable chart time labels outside the scaling SVG. Russian and English labels are provided.
## Previously published documentation

- Wiring documentation: corrected the Nextion RX/TX connections in `docs/images/connection.png`.
- PCB documentation: replaced the future-development placeholder with a bilingual overview and a link to the published OSHWLab / EasyEDA project.

## 1.0.0-beta.9 (display 1.0.4)

- Motor speed factor (Settings → Motor board → Command format, entity `Motor Speed Factor`, 0.5–3.0, default 1.0): every speed command is multiplied by it, for another motor on the same board that runs at a fraction of the commanded speed; the speed corrector trims the rest. Board max speed (SPDMAX) can now be set up to 40 km/h so the board does not cap the multiplied command.

- Web UI is installable as an app (PWA): manifest, icons from the project logo (`web-ui/pwa/`, source `icon-source.png`), iOS home-screen tags and an install-only service worker without cache. iPhone/iPad: full screen over plain http; Android/desktop: install over your own HTTPS proxy.

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
