# Building from source

[Русская версия](BUILD.ru.md) · [Back to README](../README.md)

## Two builds, one code base

All logic lives in [`esphome/packages/00-core.yaml`](../esphome/packages/00-core.yaml) and the packages it includes. The two top-level files differ only in the network part:

| File | Network package | For |
|---|---|---|
| `treadmill-public.yaml` | `91-net-public.yaml` | Releases. No secrets: Wi-Fi is entered in the `Treadmill Setup` hotspot; no API encryption; no ESPHome OTA (updates come from GitHub). |
| `treadmill.yaml` | `91-net-private.yaml` | Your own board. Wi-Fi, API encryption key, OTA password and fallback hotspot from `secrets.yaml`. Can be flashed over Wi-Fi from the ESPHome dashboard. |

Both use the device name `treadmill`, so they are the same device for ESPHome and Home Assistant.

For the private build copy [`secrets.example.yaml`](../esphome/secrets.example.yaml) to `secrets.yaml` and fill it in.

> [!NOTE]
> The public build has no ESPHome OTA, so the ESPHome dashboard cannot flash it over Wi-Fi. Flash it over USB, or install releases from the web page. The private build can be flashed over Wi-Fi, but only after it has been flashed once over USB.

## Changing the web interface

The page is written in plain HTML/CSS/JS in [`web-ui/`](../web-ui/), with no framework and no build tools apart from Python 3:

```sh
cd web-ui
python3 generate.py      # writes ../esphome/treadmill_dashboard.h
```

`generate.py` merges the sources into one page, gzips it and writes it as a C byte array into `esphome/treadmill_dashboard.h`. The firmware serves it from flash. Rebuild and flash the firmware afterwards, then reload the page with Ctrl+F5.

The generated header is committed, so building the firmware does not need Python. If you change anything in `web-ui/`, regenerate the header and commit both; CI (`web-ui/check_header.py`) fails when they differ.

`generate.py` also writes `web-ui/generated/demo.html`: the same page plus [`web-ui/demo/mock.js`](../web-ui/demo/mock.js), a virtual treadmill in the browser. CI publishes it as the [live demo](https://samsonovss.github.io/ESPHome-Treadmill-FTMS/). Open it locally with any static server to try changes without a board.

Main sources:

| File | Part |
|---|---|
| `preview/concept/index.html`, `concept-live.js` | Workout page, programs, history and progress |
| `settings.js`, `app.js` | Settings, system info, updates |
| `controls.js`, `diag.js` | Connection to the board (REST/SSE) |
| `history.js`, `fit.js` | Workout archive and FIT export |
| `routes-lib.js`, `routes-map.js` | Routes and the map |
| `style.css`, `theme.js`, `locales/*.json` | Look, themes, translations |

## Version and releases

The version is `project.version` in `packages/00-core.yaml`. The update check compares it with the release manifest, so:

- while testing on your own board, keep the version unchanged;
- before publishing, bump it (`1.0.0-beta.6`, …), build **`treadmill-public.yaml`** and download it twice from the ESPHome dashboard (*Install → Manual download*): **OTA format** and **Factory format**. Then publish both:

```sh
PRE=1 tools/fw-release.sh 1.0.0-beta.6 path/to/firmware.ota.bin path/to/firmware.factory.bin "Short summary"
```

The script creates the `fw-v<version>` pre-release with both images, refreshes the `fw-manifest` release that boards poll for Wi-Fi updates, and rebuilds the [USB install page](https://samsonovss.github.io/ESPHome-Treadmill-FTMS/install/) with the new factory image. Never reuse a published version number.
