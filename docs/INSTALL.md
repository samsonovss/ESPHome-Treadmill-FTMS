# Installation

[Русская версия](INSTALL.ru.md) · [Back to README](../README.md)

## 1. First install: USB only

You need an ESP32-S3 with 16 MB flash and 8 MB octal PSRAM (`N16R8`). The firmware uses its own flash layout: 16 MB with a separate partition for the workout archive. A board running an older build or another firmware **cannot be converted over Wi-Fi**: the first flash must be done over USB. All later updates come over Wi-Fi.

**Ready-made firmware (recommended).** No ESPHome install needed.

1. Connect the ESP32-S3 to the computer with a USB data cable (the native USB port).
2. Open the **[install page](https://samsonovss.github.io/ESPHome-Treadmill-FTMS/install/)** in Chrome or Edge and press *Connect and install*. Agree to erase the board if it ran other firmware.
3. No browser support? Download `treadmill-fw-<version>.factory.bin` from the [latest release](https://github.com/samsonovss/ESPHome-Treadmill-FTMS/releases) and flash it with [ESPHome Web](https://web.esphome.io) or `esptool.py write_flash 0x0 treadmill-fw-<version>.factory.bin`.

If no port appears, hold **BOOT**, press **RESET**, release **BOOT** and try again.

**Build it yourself** (to change the code):

1. Install [ESPHome](https://esphome.io/guides/getting_started_hassio/) (Home Assistant add-on or `pip install esphome`).
2. Copy the whole [`esphome/`](../esphome/) directory into your ESPHome configuration directory. Keep `packages/`, `components/` and the `*.h` files next to the yaml files.
3. In the ESPHome dashboard open **`treadmill-public.yaml`** and choose *Install → Plug into this computer* (or run `esphome run treadmill-public.yaml`).

The public build contains no secrets and no Wi-Fi password, so it works for anyone as is. If you want your own Wi-Fi, API key and OTA password inside the firmware, use the private build instead; see [building from source](BUILD.md).

## 2. Wi-Fi setup

1. After the flash the board opens the **`Treadmill Setup`** Wi-Fi hotspot (open, no password).
2. Join it from a phone or laptop. The setup page opens by itself; if not, go to `http://192.168.4.1`.
3. Pick your home network, enter the password and save. The board reboots and joins your network.

The Wi-Fi settings are stored in the board's flash and **survive firmware updates**. Bluetooth stays off while the board is in setup mode, so the hotspot is stable. Give it about 30 seconds after power-on.

## 3. Open the web interface

Open `http://treadmill.local` in a browser, or the IP address your router shows for the `treadmill` device. The interface works on a phone, tablet or computer.

## 4. First checks (without a person on the belt)

- **Stop** stops the belt;
- the minimum speed is gentle;
- the incline returns to zero;
- speed and incline limits match your treadmill.

Then go to *Settings* and enter your profile (age, weight, max heart rate). Heart-rate zones and calories depend on it.

## 5. Heart-rate strap

*Settings → HR monitor*: put the strap on, start the scan, pick your strap. The choice is stored on the board and restored after reboots and updates.

## 6. Updates

*Settings → System* shows:

- **ESP firmware**: the treadmill firmware. The board checks GitHub at boot and every 6 hours; **Check** checks right now. When a new version is out, the button **Update to v…** appears together with a blue chip icon in the header. The belt must be stopped. The board downloads the firmware from GitHub, installs it and reboots in 1-2 minutes. Wi-Fi, history and the strap pairing are kept.
- **Display firmware**: the Nextion display. When a new version is out, a red monitor icon appears in the header and **Update to v…** in the row. The board downloads the `.tft` and writes it to the display; this takes a few minutes. Do not cut the power.

Firmware is published as **pre-releases** (beta) on the [Releases](https://github.com/samsonovss/ESPHome-Treadmill-FTMS/releases) page.

## Home Assistant (optional)

The treadmill is an ordinary ESPHome device: Home Assistant discovers it on the network (*Settings → Devices & services*). The public build has no API encryption key, so it adds without a key. If you need encryption, use the private build.

## Troubleshooting

| Symptom | What to do |
|---|---|
| No `Treadmill Setup` hotspot | Wait ~30 s after power-on. If the board already knows a Wi-Fi network, it tries that first; the hotspot appears only when it cannot connect (about a minute later). |
| `treadmill.local` does not open | Use the IP address from your router; some networks and Android devices do not resolve `.local` names. |
| Update button does nothing | Stop the belt first; updates are refused while it runs. |
| Wi-Fi update fails right after flashing an old build | That board still has the old flash layout: flash once over USB (step 1). |
