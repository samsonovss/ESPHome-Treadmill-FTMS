# Запись тренировок беговой дорожки

Это дополнительный модуль Home Assistant, который записывает тренировки в CSV и после остановки создаёт файл активности Garmin FIT.

## Как это работает

1. Автоматизация Home Assistant запускает `treadmill_recorder.py`, когда `binary_sensor.treadmill_treadmill_usage` переходит в состояние `on`.
2. Раз в секунду скрипт получает значения сущностей через локальный REST API Home Assistant.
3. Измерения записываются в CSV с датой и временем в имени.
4. После остановки дорожки создаётся итоговый JSON.
5. `treadmill_fit_export.py` преобразует CSV и JSON в FIT-активность типа «бег на дорожке».

Экспортёр FIT использует только стандартную библиотеку Python. В файл записываются время, пульс, скорость, дистанция, наклон, калории и итоговые сообщения lap/session/activity.

## Структура каталога

```text
/config/treadmill_workouts/
├── scripts/
│   ├── treadmill_recorder.py
│   └── treadmill_fit_export.py
├── secrets/
│   └── .ha_token
├── workouts/
├── logs/
└── state/
```

В репозитории runtime-каталоги пустые. Токены, логи, состояние и личные файлы тренировок исключены через `.gitignore`.

## Установка

1. Скопируйте `treadmill_workouts/` в `/config/treadmill_workouts/` Home Assistant.
2. Сделайте скрипты исполняемыми:

   ```sh
   chmod 755 /config/treadmill_workouts/scripts/*.py
   ```

3. Создайте долгоживущий токен в профиле пользователя Home Assistant.
4. Сохраните только значение токена в файл:

   ```text
   /config/treadmill_workouts/secrets/.ha_token
   ```

5. Ограничьте доступ к нему:

   ```sh
   chmod 600 /config/treadmill_workouts/secrets/.ha_token
   ```

6. Добавьте содержимое [`examples/configuration.yaml`](examples/configuration.yaml) в `configuration.yaml`.
7. Добавьте [`examples/automations.yaml`](examples/automations.yaml) в YAML автоматизаций или создайте три автоматизации через интерфейс Home Assistant.
8. Проверьте конфигурацию и перезапустите Home Assistant.

## Настройка

Если ваша установка отличается, измените константы в `scripts/treadmill_recorder.py`:

- `BASE_URL` — локальный адрес Home Assistant;
- `ROOT` — каталог рекордера;
- `TZ` — часовой пояс тренировки;
- `LIVE_ENTITIES` — сущности, опрашиваемые каждую секунду;
- `SUMMARY_ENTITIES` — итоговые показатели тренировки.

Указанные в скрипте entity_id соответствуют этому проекту дорожки.

## Результат

Для каждой тренировки создаются:

```text
YYYY-MM-DD_HH-MM-SS.csv
YYYY-MM-DD_HH-MM-SS.summary.json
YYYY-MM-DD_HH-MM-SS.fit
```

Столбцы CSV:

```text
timestamp, heart_rate, speed_kmh, incline_pct, distance_km,
time_s, calories, status, program, zone
```

FIT можно импортировать в приложения, поддерживающие стандартные FIT-активности.

## Ручной запуск

```sh
python3 /config/treadmill_workouts/scripts/treadmill_recorder.py start
python3 /config/treadmill_workouts/scripts/treadmill_recorder.py stop
python3 /config/treadmill_workouts/scripts/treadmill_recorder.py status
```

Ручное преобразование существующего CSV:

```sh
python3 \
  /config/treadmill_workouts/scripts/treadmill_fit_export.py \
  workout.csv workout.summary.json workout.fit
```

Лог находится в `/config/treadmill_workouts/logs/recorder.log`.

## Конфиденциальность

Не публикуйте `.ha_token`, CSV, FIT, summary, state и log-файлы. Экспорт тренировок может содержать время, пульс, скорость, дистанцию и другие личные данные.
