# Guide to Reading and Decoding UART Data for a Treadmill

## Step 1: Connecting to the Upper Control Board
- **Photo**: Image of the upper control board of the treadmill.  
  The UART connector (usually 6-pin, with RX and TX) is circled in the photo.  
  <img src="/docs/images/uart.png" alt="[photo of control board]" width="400"/>
- **Key Connection Notes**:  
  - The upper board ("control board") sends commands to the lower board approximately every half second.  
  - The lower board only sends confirmations as feedback and does not initiate data itself.  
  - To read commands, connect specifically to the upper board!
- **Connection**:  
  - Use an ESP32-S3 to connect to the treadmill's board.  
  - **GPIO17 (TX)**: Connect to RX (e.g., Pin 5 on the connector) via a level shifter (5V → 3.3V).  
  - **GPIO18 (RX)**: Connect to TX (e.g., Pin 4 on the connector) via a level shifter.  
  - **GND**: Common ground with the level shifter and the board.  
  - **Power**: Use an LM2596S to convert 12V to 5V, then supply 3.3V to the ESP32-S3.

## Step 2: Reading Raw UART Data
- **Goal**: Capture raw data sent by the control board, encoded in ASCII.
- **UART Setup**:  
  - Start with a basic ESPHome configuration to capture all messages without noise filtering.  
  - Use the following code to output raw data:
  ```yaml
  uart:
    - id: uart_bus
      tx_pin: 
        number: GPIO17
        inverted: false
      rx_pin: 
        number: GPIO18
        mode: 
          input: true
          pullup: true
        inverted: false
      baud_rate: 5000
      data_bits: 8
      parity: NONE
      stop_bits: 1
      rx_buffer_size: 1024
      debug:
        direction: RX
        dummy_receiver: false
        after:
          timeout: 100ms
          bytes: 30
        sequence:
          - lambda: |-
              std::vector<uint8_t> filtered_bytes;
              for (auto byte : bytes) {
                if (byte != 0x00) {
                  filtered_bytes.push_back(byte);
                }
              }
              if (!filtered_bytes.empty()) {
                UARTDebug::log_string(direction, filtered_bytes);
              }
  ```

- **Selecting UART Baud Rate**:  
  - The baud rate is unknown and needs to be determined.  
  - Add a slider to the ESPHome configuration to adjust the speed, step 100: (in my case, the speed was 4800 - 5000)
  ```yaml
  number:
    - platform: template
      name: "UART Baud Rate"
      id: uart_baud_rate
      min_value: 1000
      max_value: 115200
      step: 100
      initial_value: 5000
      optimistic: true
      restore_value: yes
      entity_category: config
      icon: "mdi:swap-horizontal"
      on_value:
        - lambda: |-
            id(uart_bus).flush();
            uint32_t new_baud_rate = (uint32_t)x;
            ESP_LOGD("UART", "Changing baud rate from %i to %i", id(uart_bus).get_baud_rate(), new_baud_rate);
            if (id(uart_bus).get_baud_rate() != new_baud_rate) {
              id(uart_bus).set_baud_rate(new_baud_rate);
              id(uart_bus).load_settings();
            }
  ```
- **Logging in Debug Mode**:
  ```yaml
  logger:
    level: DEBUG
  ```

- **Process**:  
  - Adjust the slider, changing the speed (e.g., from 1000 to 115200 with a step of 100).  
  - Monitor the logs in ESPHome (via `logger`).  
  - If raw ASCII commands start appearing, the connection and baud rate are correct.  
  - Note this speed (in my case, for example, 4900 or 5000).

## Step 3: Decoding Data from ASCII to Text
- **Raw Data**: From the previous step, you obtained byte streams, possibly with noise (0x00). These are encoded in ASCII, where each byte represents a character (e.g., 0x5B = `[`, 0x30 = `0`).
- **How to Decode in Real Life**:  
  - **View Bytes**: In the logs, you’ll see hex values, e.g., `5B 53 45 54 53 50 44 3A 30 31 30 5D`.  
  - **Decoding with Converters**:  
    - Use online tools like a "Hex to ASCII" converter.  
    - Input the hex string (e.g., `5B 53 45 54 53 50 44 3A 30 31 30 5D`).  
    - Result: text, e.g., `[SETSPD:010]`, where the command indicates a speed of 1.0 km/h.  
  - **Manual Decoding**:  
    - Find an ASCII table (e.g., on ascii-code.com).  
    - Convert each hex byte to a character:  
      - `5B` → 91 → `[`  
      - `53` → 83 → `S`  
      - `45` → 69 → `E`  
      - `54` → 84 → `T`  
      - `53` → 83 → `S`  
      - `50` → 80 → `P`  
      - `44` → 68 → `D`  
      - `3A` → 58 → `:`  
      - `30` → 48 → `0`  
      - `31` → 49 → `1`  
      - `30` → 48 → `0`  
      - `5D` → 93 → `]`  
    - Result: `5B 53 45 54 53 50 44 3A 30 31 30 5D` → `[SETSPD:010]`.  
  - **Interpretation**: The command `[SETSPD:010]` means a speed of 1.0 km/h, `[SETINC:000]` means an incline of 0%. Ignore noise (0x00).  
- **Filtering and Decoding in Code**:  
  - Remove noise (0x00) and convert to readable form.  
  - This code parses commands like `[SETSPD:010]` and `[SETINC:000]`. If your command names differ, replace them with your own:
  ```yaml
  uart:
    - id: uart_bus
      tx_pin: 
        number: GPIO17
        inverted: false
      rx_pin: 
        number: GPIO18
        mode: 
          input: true
          pullup: true
        inverted: false
      baud_rate: 4900
      data_bits: 8
      parity: NONE
      stop_bits: 1
      rx_buffer_size: 512
      debug:
        direction: RX
        dummy_receiver: false
        after:
          timeout: 100ms
          bytes: 30
        sequence:
          - lambda: |-
              char message[31] = {0};
              int len = 0;
              for (size_t i = 0; i < std::min(bytes.size(), size_t(30)); i++) {
                if (bytes[i] != 0x00) {
                  message[len++] = bytes[i];
                }
              }
              if (len > 0) {
                ESP_LOGD("UART", "Received: %s", message);
                auto parse_command = [&](const char* start) {
                  if (start == nullptr || *start != '[') return;
                  const char* end = strchr(start, ']');
                  if (end == nullptr || end <= start + 1) return;
                  const char* colon = strchr(start + 1, ':');
                  if (colon == nullptr || colon >= end) return;
                  char command_name[16] = {0};
                  char command_value[16] = {0};
                  int name_len = colon - (start + 1);
                  int value_len = end - (colon + 1);
                  if (name_len >= sizeof(command_name) || value_len >= sizeof(command_value)) {
                    ESP_LOGW("UART", "Command or value too long: %s", start);
                    return;
                  }
                  memcpy(command_name, start + 1, name_len);
                  memcpy(command_value, colon + 1, value_len);
                  ESP_LOGD("UART", "Command: %s, Value: %s", command_name, command_value);
                  if (strcmp(command_name, "SETSPD") == 0) {
                    int speed = 0;
                    bool valid = true;
                    for (int i = 0; command_value[i] != '\0' && i < 5; i++) {
                      if (command_value[i] < '0' || command_value[i] > '9') {
                        valid = false;
                        break;
                      }
                      speed = speed * 10 + (command_value[i] - '0');
                    }
                    if (valid) {
                      id(treadmill_speed_feedback) = speed;
                      ESP_LOGD("UART", "Speed updated: %d", speed);
                    } else {
                      ESP_LOGW("UART", "Invalid speed: %s", command_value);
                    }
                  }
                  else if (strcmp(command_name, "SETINC") == 0) {
                    int incline = 0;
                    bool valid = true;
                    for (int i = 0; command_value[i] != '\0' && i < 5; i++) {
                      if (command_value[i] < '0' || command_value[i] > '9') {
                        valid = false;
                        break;
                      }
                      incline = incline * 10 + (command_value[i] - '0');
                    }
                    if (valid) {
                      id(treadmill_incline_feedback) = incline;
                      ESP_LOGD("UART", "Incline updated: %d", incline);
                    } else {
                      ESP_LOGW("UART", "Invalid incline: %s", command_value);
                    }
                  }
                  else {
                    ESP_LOGD("UART", "Unknown command processed: %s=%s", command_name, command_value);
                  }
                };
                const char* start = strchr(message, '[');
                parse_command(start);
                if (start == nullptr) {
                  ESP_LOGD("UART", "No commands found in format [COMMAND:VALUE]: %s", message);
                }
              }
  ```
- **Result**:  
  - Raw data is filtered, and commands like `[SETSPD:010]` (speed 1 km/h) or `[SETINC:000]` (incline 0%) are decoded.  
  - Feedback values are stored in variables, e.g., `treadmill_speed_feedback` and `treadmill_incline_feedback`, for further use.

## Summary
- Connect to UART as shown in the photo and read raw data, adjusting the baud rate with the slider.  
- When raw ASCII data starts coming in:  
- Decode the data from ASCII using tools like a converter or manually (with an ASCII table), filtering noise and extracting commands to obtain readable speed and incline values.  
- This is my method for decoding UART for a treadmill!
