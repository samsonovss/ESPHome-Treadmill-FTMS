# Patched ESPHome BLE Server Component

This directory vendors the `esp32_ble_server` component from ESPHome 2026.5.3.

Source:

<https://github.com/esphome/esphome/tree/2026.5.3/esphome/components/esp32_ble_server>

## Local modification

Only `ble_characteristic.cpp` differs from the upstream 2026.5.3 component.

ESPHome 2026.5.3 changes subscribed Indicate messages into unconfirmed Notify messages. Zwift requires a confirmed FTMS Control Point response before it accepts the treadmill as a controllable device.

The local patch preserves the client's CCCD choice:

- Notify subscriptions use unconfirmed notifications;
- Indicate subscriptions use confirmed indications;
- the ESP-IDF GATT server handles the confirmation exchange.

Remove the local component and the matching `external_components` entry when the upstream ESPHome implementation supports confirmed indications for this use case.

## License

The vendored files retain the ESPHome licensing model:

- Python and other non-runtime files: MIT;
- C/C++ runtime files: GPLv3.

See [LICENSE](LICENSE).
