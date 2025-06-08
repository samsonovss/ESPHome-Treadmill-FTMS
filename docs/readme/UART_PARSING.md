# Инструкция по считыванию и декодированию данных UART для беговой дорожки

## Шаг 1: Подключение к верхнему бортовому компьютеру
- **Фотография**: Изображение верхнего бортового компьютера беговой дорожки.
  [фото бортового компьютера]  
  Разъем UART (обычно 6-контактный, с RX и TX) обведен на фото.
- **Важно о платах**:  
  - Верхняя плата ("бортовой компьютер") отправляет команды нижней плате с интервалом примерно в полсекунды.
  - Нижняя плата только подтверждает получение, и отправляет только ответ в виде обратной связи.
  - Для считывания команд подключайтесь именно к верхней плате.
- **Подключение**:  
  - Используйте ESP32-S3 для подключения к плате беговой дорожки.  
  - **GPIO17 (TX)**: Подключите к RX (например, Pin 5) на плате через преобразователь уровня (5В → 3.3В).  
  - **GPIO18 (RX)**: Подключите к TX (например, Pin 4) на плате через преобразователь уровня.  
  - **GND**: Общий провод с преобразователем уровня и платой.  
  - **Питание**: Используйте LM2596S для преобразования 12В в 5В, затем подключите 3.3В к ESP32-S3.

## Шаг 2: Считывание сырых данных UART
- **Цель**: Получить сырые данные, отправляемые бортовым компьютером, они закодированы в ASCII.
- **Настройка UART**:  
  - Начните с базовой конфигурации в ESPHome для захвата всех сообщений без фильтрации шума.  
  - Используйте следующий код для вывода сырых данных:
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

- **Подбор скорости UART (Baud Rate)**:  
  - Скорость передачи (baud rate) неизвестна, поэтому ее нужно определить.  
  - Добавьте в конфигурацию ESPHome слайдер для подбора скорости, шаг 100: (в моем случае скорость 4800 - 5000)
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
- **лог в режиме (Debug)**:
  ```yaml
  logger:
    level: DEBUG
  ```

- **Процесс**:  
  - Двигайте слайдер, изменяя скорость (например, от 1000 до 115200 с шагом 100).  
  - Наблюдайте за логами в ESPHome (через `logger`).  
  - Если начинают поступать сырые команды в ASCII значит подключение и подбор скорость коректны.  
  - Запомните эту скорость (в моем случае, например, 4900 или 5000).

## Шаг 3: Декодирование данных из ASCII в текст
- **Сырые данные**: На предыдущем шаге вы получили потоки байтов, возможно, с шумом (0x00). Эти данные закодированы в ASCII, где каждый байт представляет символ (например, 0x5B = `[`, 0x30 = `0`).
- **Как декодировать в реальной жизни**:  
  - **Просмотр байтов**: В логах вы увидите hex-значения, например, `5B 53 45 54 53 50 44 3A 30 31 30 5D`.  
  - **Декодирование с помощью конвертеров**:  
    - Используйте онлайн-инструменты, такие как "Hex to ASCII" конвертер.  
    - Вставьте hex-строку (например, `5B 53 45 54 53 50 44 3A 30 31 30 5D`).  
    - Результат: текст, например, `[SETSPD:010]`, где команда означает скорость 1.0 км/ч.  
  - **Ручное декодирование**:  
    - Найдите таблицу ASCII (например, на ascii-code.com).  
    - Преобразуйте каждый hex-байт в символ:  
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
    - Итог: `5B 53 45 54 53 50 44 3A 30 31 30 5D` → `[SETSPD:010]`.  
  - **Интерпретация**: Команда `[SETSPD:010]` — это скорость 1.0 км/ч, `[SETINC:000]` — наклон 0%. Игнорируйте шум (0x00).  
- **Фильтрация и декодирование в коде**:  
  - Очистите данные от шума (0x00) и преобразуйте в читаемый вид.  
  - Данный код использует парсинг команд `[SETSPD:010]` и `[SETINC:000]` , если ваше название команд отличается замените на свои: 
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
                ESP_LOGD("UART", "Получено: %s", message);
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
                    ESP_LOGW("UART", "Слишком длинная команда или значение: %s", start);
                    return;
                  }
                  memcpy(command_name, start + 1, name_len);
                  memcpy(command_value, colon + 1, value_len);
                  ESP_LOGD("UART", "Команда: %s, Значение: %s", command_name, command_value);
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
                      ESP_LOGD("UART", "Скорость обновлена: %d", speed);
                    } else {
                      ESP_LOGW("UART", "Некорректная скорость: %s", command_value);
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
                      ESP_LOGD("UART", "Наклон обновлен: %d", incline);
                    } else {
                      ESP_LOGW("UART", "Некорректный наклон: %s", command_value);
                    }
                  }
                  else {
                    ESP_LOGD("UART", "Неизвестная команда обработана: %s=%s", command_name, command_value);
                  }
                };
                const char* start = strchr(message, '[');
                parse_command(start);
                if (start == nullptr) {
                  ESP_LOGD("UART", "Не найдено команд в формате [КОМАНДА:ЗНАЧЕНИЕ]: %s", message);
                }
              }
  ```
- **Результат**:  
  - Сырые данные фильтруются, и команды вроде `[SETSPD:010]` (скорость 1 км/ч) или `[SETINC:000]` (наклон 0%) декодируются.  
  - Значения сохраняются в переменные, например, `treadmill_speed_feedback` и `treadmill_incline_feedback`, для дальнейшего использования.

## Итог
- Подключитесь к UART, как показано на фото, и считайте сырые данные, подбирая скорость (baud rate) с помощью слайдера.  
- Когда данные начнут поступать в сыром виде ASCII
- Декодируйте данные из ASCII через помощью инструментов - конвертера или вручную (с таблицей ASCII), фильтруя шум и извлекая команды для получения читаемых значений скорости и наклона.  
- Это мой метод дешифровки UART для беговой дорожки
