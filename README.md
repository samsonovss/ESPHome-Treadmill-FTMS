# ESPHome Treadmill with Zwift and Future FTMS Support

**[Русская версия / Russian version](README.ru.md)**

## About the Project
Turn your old treadmill into a smart training companion! ESPHomeTreadmill is an onboard computer based on the ESP32 S3 with ESPHome firmware, adding support for Zwift, intelligent heart rate-based programs, and UART control. It’s perfect for treadmills with PSA(xx) series boards but flexible enough to adapt to any UART-enabled treadmill. Minimal cost, maximum potential!

## How It Works
The project leverages the ESP32 S3 to communicate with the treadmill’s board (e.g., PSA(xx)) via UART. Commands like `[Setspeed:010]` (1 km/h) or `[Setincline:000]` (0%) were discovered by analyzing traffic with a UART logger. The microcontroller processes this data, converts it into real speed and incline values, and transmits them via Bluetooth Low Energy (BLE) to apps like Zwift or stores them locally for analysis in Grafana.

A heart rate monitor connects via BLE, providing pulse data. Real-time intelligent algorithms analyze the heart rate and smoothly adjust the treadmill’s settings to maintain the target training zone. For example, if your pulse drifts outside the goal, the speed adjusts automatically, delivering a personalized and effective workout.

### Advantages
- **Flexibility**: Works with any UART-supporting treadmill.
- **Modernity**: Built on the powerful ESP32 S3 microcontroller.
- **Affordability**: Requires minimal components.

## Recommended Hardware
- **ESP32 S3** (highly recommended for performance and BLE support).
- **LM2596S**: Voltage converter from 12V to 5V (non-isolated).
- **2-channel level shifter**: To match 5V (PSA(xx)) and 3.3V (ESP32 S3).
- **Treadmill**: Ideally with a PSA(xx) board, but any UART-capable model (RX-TX) will do.

## Features
### Core Functions
- **Zwift Support**: Full integration with the popular platform.
- **Heart Rate Monitor**: Connection and zone calculation based on age and gender.
- **Real Data**: Incline in percentages and speed calibration.
- **Button Control**: Speed and incline adjustment via GPIO (with feedback).
- **Manual Mode**: Training without a heart rate monitor.
- **Local Storage**: Save runs and visualize them in Grafana.

### Smart Adjustment
- **Pulse Maintenance**: Speed adjusts smoothly based on the difference from the target zone:
  - Difference > 20 bpm: ±0.5 km/h in 0.1 steps every 2 seconds.
  - Difference < 20 bpm: ±0.1 km/h every 20 seconds.

### Warm-Up
- **Smart Warm-Up**: Brings pulse to Zone 1 to prepare ligaments.
- **Customizable Time**: Waits for Zone 1, then completes based on a timer (e.g., 5 minutes).

### Cool-Down
- **Smooth Reduction**: Lowers speed until pulse returns to Zone 1.
- **Customizable Time**: Mirrors the warm-up logic.

### Training Programs
- **Custom Zone**: Maintains a set pulse zone via speed adjustments.
- **Fat Burning**: Zone 2 with smooth speed and incline control.
- **Interval**: Switches between Zones 1 and 4 with auto-speed tuning.
- **Recovery**: Keeps Zone 1 for light running.

## Future Plans
- FTMS standard support for Kinomap and iFit compatibility.
- Interactive elevation maps and new training programs.
- Display with a user-friendly interface.
- Auto-incline support in Zwift.
- All-in-one board design for easier assembly.
- ESPHome component for seamless ecosystem integration.
- Distance sensor (for button-free speed control).
- MQTT data transmission (for integration beyond Home Assistant).

## Authors
Created by [@samsonovss](https://t.me/samsonovss) in collaboration with Grok, an AI developed by xAI.

## Support the Project
If you’d like to support this project and help me keep it growing, consider buying me a coffee! You can send a donation via cryptocurrency:
- **BTC**: `bc1q3cza0kasutzes4hfddxuclmd9ghn5v7zw2nr5c`  
- **USDT (TRC-20)**: `0x5dd5a346Dd64dfE938a60D7b24b633b1ACE01719`  
Every little bit helps — thank you! 

