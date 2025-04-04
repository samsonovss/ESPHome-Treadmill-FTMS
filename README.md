# ESPHome Treadmill with Zwift and Future FTMS Support

**[Русская версия / Russian version](README.ru.md)**

## About the Project
Turn your old treadmill into a smart training companion! ESPHomeTreadmill is an onboard computer based on the ESP32 S3 with ESPHome firmware, adding support for Zwift, intelligent heart rate-based programs, and UART control. It’s perfect for treadmills with PSA(xx) series boards but flexible enough to adapt to any UART-enabled treadmill. Minimal cost, maximum potential!

## How It Works
The project leverages the ESP32 S3 to communicate with the treadmill’s board (e.g., PSA(xx)) via UART. Commands like `[SETSPD:010]` (1 km/h) or `[SETINC:000]` (0%) were discovered by analyzing traffic with a UART logger. The microcontroller processes this data, converts it into real speed and incline values, and transmits them via Bluetooth Low Energy (BLE) to apps like Zwift or stores them locally for analysis in Grafana.

A heart rate monitor connects via BLE, providing pulse data. Real-time intelligent algorithms analyze the heart rate and smoothly adjust the treadmill’s settings to maintain the target training zone. For example, if your pulse drifts outside the goal, the speed adjusts automatically, delivering a personalized and effective workout.

### Advantages
- **Flexibility**: Works with any UART-supporting treadmill.
- **Modernity**: Built on the powerful ESP32 S3 microcontroller.
- **Affordability**: Requires minimal components.

## Recommended Hardware
- **ESP32 S3** (highly recommended for performance and BLE support).
- **LM2596S**: Voltage converter from 12V to 5V (non-isolated).
- **2-channel level shifter**: To match 5V (PSA(xx)) and 3.3V (ESP32 S3).
- **Treadmill**: Ideally with a **[PSA(xx) board](image/PSA(XX)H.jpg)**, but any UART-capable model (RX-TX) will do.
 ![Treadmill Screenshot](image/PSA(XX)H.jpg)
## Connection
- ESP32 S3:
  - GPIO17 (TX): Transmits data to RX (Pin 5) on PSA(xx) through a level shifter.
  - GPIO18 (RX): Receives data from TX (Pin 4) on PSA(xx) through a level shifter.
  - GND: Common ground with the level shifter (3.3V side).
  - 3.3V: Power supply for the Low Voltage (LV) side of the level shifter.
- ESP32 S3 (Power Supply Connections):
  - LV (Low Voltage): 3.3V side connected to the ESP32.
  - HV (High Voltage): 5V side connected to PSA(xx).
  - GND (LV): Ground from the ESP32.
  - Vcc (LV): 3.3V from the ESP32.
  - GND (HV): Ground from the LM2596S.
  - Vcc (HV): 5V from the LM2596S.
- PSA(xx) Board (6-pin):
  - Pin 1 (12V): Supplies power to the board, feeds the input of the LM2596S, and connects to Pin 6 (SW).
  - Pin 2: Not connected (unused).
  - Pin 3 (GND): Common ground with the LM2596S and the level shifter.
  - Pin 4 (TX): Transmits data to GPIO18 (RX) on the ESP32 through the level shifter.
  - Pin 5 (RX): Receives data from GPIO17 (TX) on the ESP32 through the level shifter.
  - Pin 6 (SW): Connected to Pin 1 (12V) to power on the treadmill.
- PSA(xx) Board (Additional 6-pin Section):
  - Input 12V: Receives power from Pin 1 (12V) of PSA(xx).
  - Output 5V: Provides power to the Vcc (HV) side of the level shifter.
  - GND: Common ground with PSA(xx) and the level shifter.
## Features
### Core Functions
- **Zwift Support**: Full integration with the popular platform.
![ESPHome Treadmill Zwift](image/Zwift.gif)
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

### Hassio Interface
<table>
  <tr>
    <td><img src="image/int2.png" alt="Screenshot 1" width="300"/></td>
    <td><img src="image/int.png" alt="Screenshot 2" width="300"/></td>
  </tr>
</table>

## ESPHome Setup
The file (config.yaml) configures the ESP32 S3 to control the treadmill and connect the heart rate monitor.
- UART configuration for communicating with the treadmill (used to send and receive commands)
```yaml
uart:
  tx_pin: GPIO17    # Передача данных (TX) на GPIO17
  rx_pin: GPIO18    # Приём данных (RX) на GPIO18

# Bluetooth Low Energy (BLE) configuration for connecting the heart rate monitor
ble_client:
  - mac_address: "XX:XX:XX:XX:XX:XX"  # Replace with your heart rate monitor's MAC address
```

## Future Plans
- FTMS standard support for Kinomap and iFit compatibility.
- Interactive elevation maps and new training programs.
- Display with a user-friendly interface.
- Auto-incline support in Zwift.
- All-in-one board design for easier assembly.
- ESPHome component for seamless ecosystem integration.
- Distance sensor (for button-free speed control).
- MQTT data transmission (for integration beyond Home Assistant).
- Web interface for control without Home Assistant integration.

## Authors
Created by [@samsonovss](https://t.me/samsonovss) in collaboration with Grok, an AI developed by xAI.

## Support the Project
If you’d like to support this project and help me keep it growing, consider buying me a coffee! You can send a donation via cryptocurrency:
- **BTC**: `bc1q3cza0kasutzes4hfddxuclmd9ghn5v7zw2nr5c`  
- **USDT (TRC-20)**: `0x5dd5a346Dd64dfE938a60D7b24b633b1ACE01719`
  
Any support matters — thank you!

