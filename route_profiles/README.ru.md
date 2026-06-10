# Профили маршрутов

[English version](README.md)

Этот каталог содержит исходные маршруты и инструменты разработки для
подготовки автономных профилей наклона беговой дорожки.

Для обычной установки ESPHome он **не требуется**. Готовые профили, которые
использует прошивка, уже находятся в
[`esphome/incline_profiles.h`](../esphome/incline_profiles.h).

## Структура

```text
route_profiles/
├── gpx_files/       # Исходные GPX с точками дистанции и высоты
├── scripts/         # Инструменты преобразования GPX
└── zwift_profiles/  # Сохранённые профили Zwift и фрагменты C++
```

## Подготовка маршрута

1. Создайте или экспортируйте маршрут в KML либо GPX.
2. Если в файле нет высоты, обработайте его через
   [GPS Visualizer](https://www.gpsvisualizer.com/convert_input):
   - формат результата: `GPX`;
   - данные высоты DEM: `Best available source`;
   - точки с равным интервалом: `30 meters`.
3. Сохраните результат в `route_profiles/gpx_files/`.
4. Запустите конвертер из корня репозитория:

   ```sh
   python3 route_profiles/scripts/gpx_to_incline_profile.py \
     route_profiles/gpx_files/example.gpx \
     --name example_route
   ```

По умолчанию рядом с исходным файлом будет создан
`example.profile.h`. Другой путь можно указать через `--output`.

Результат имеет тот же двумерный формат, который использует ESPHome:

```cpp
const float incline_profile_example_route[][2] PROGMEM = {
  {0.000, 0},
  {0.030, 3},
};
```

Первое значение — пройденная дистанция в километрах, второе — уровень наклона
дорожки.

## Калибровка

По умолчанию используется соответствие:

```text
максимальный уровень дорожки = 15
реальный уклон на максимуме = 5%
```

Для другой дорожки задайте собственные значения:

```sh
python3 route_profiles/scripts/gpx_to_incline_profile.py \
  route_profiles/gpx_files/example.gpx \
  --name example_route \
  --maximum-level 15 \
  --real-grade-at-maximum 5
```

Проверьте созданный профиль перед переносом в
`esphome/incline_profiles.h`. Конвертер специально не перезаписывает рабочий
файл прошивки автоматически.

## Требования к GPX

Конвертер ожидает точки `<wpt>`, содержащие:

- высоту в поле `<ele>`;
- поле `<name>`, которое начинается с дистанции в метрах и содержит слово
  `distance`.

Такие точки умеет создавать GPS Visualizer.
