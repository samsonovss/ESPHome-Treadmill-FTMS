# Сборка из исходников

[English version](BUILD.md) · [Назад к README](../README.ru.md)

## Две сборки, один код

Вся логика лежит в [`esphome/packages/00-core.yaml`](../esphome/packages/00-core.yaml) и подключаемых им пакетах. Два основных файла отличаются только сетевой частью:

| Файл | Сетевой пакет | Для чего |
|---|---|---|
| `treadmill-public.yaml` | `91-net-public.yaml` | Релизы. Без секретов: Wi-Fi вводится в точке `Treadmill Setup`; без шифрования API; без ESPHome OTA (обновления идут с GitHub). |
| `treadmill.yaml` | `91-net-private.yaml` | Своя плата. Wi-Fi, ключ шифрования API, пароль OTA и запасная точка доступа из `secrets.yaml`. Прошивается по Wi-Fi из панели ESPHome. |

У обеих имя устройства `treadmill`, поэтому для ESPHome и Home Assistant это одно устройство.

Для приватной сборки скопируйте [`secrets.example.yaml`](../esphome/secrets.example.yaml) в `secrets.yaml` и заполните.

> [!NOTE]
> В публичной сборке нет ESPHome OTA, поэтому панель ESPHome не может прошить её по Wi-Fi. Прошивайте по USB или ставьте релизы со страницы дорожки. Приватную сборку можно шить по Wi-Fi, но только после одной прошивки по USB.

## Изменение веб-интерфейса

Страница написана на обычных HTML/CSS/JS в [`web-ui/`](../web-ui/), без фреймворков и сборщиков, нужен только Python 3:

```sh
cd web-ui
python3 generate.py      # пишет ../esphome/treadmill_dashboard.h
```

`generate.py` склеивает исходники в одну страницу, сжимает gzip и записывает массивом байт в `esphome/treadmill_dashboard.h`. Прошивка отдаёт её из флеш-памяти. После этого пересоберите и прошейте прошивку, затем обновите страницу через Ctrl+F5.

Готовый заголовок лежит в репозитории, поэтому для сборки прошивки Python не нужен. Если меняете что-то в `web-ui/`, перегенерируйте заголовок и коммитьте оба; проверка в CI (`web-ui/check_header.py`) упадёт, если они расходятся.

`generate.py` также пишет `web-ui/generated/demo.html`: ту же страницу плюс [`web-ui/demo/mock.js`](../web-ui/demo/mock.js) — виртуальную дорожку в браузере. CI публикует её как [живое демо](https://samsonovss.github.io/ESPHome-Treadmill-FTMS/). Локально её можно открыть через любой статический сервер и пробовать изменения без платы.

Основные исходники:

| Файл | Что внутри |
|---|---|
| `preview/concept/index.html`, `concept-live.js` | Страница тренировки, программы, история и прогресс |
| `settings.js`, `app.js` | Настройки, сведения о системе, обновления |
| `controls.js`, `diag.js` | Связь с платой (REST/SSE) |
| `history.js`, `fit.js` | Архив тренировок и экспорт в FIT |
| `routes-lib.js`, `routes-map.js` | Маршруты и карта |
| `style.css`, `theme.js`, `locales/*.json` | Оформление, темы, переводы |

## Версия и релизы

Версия — это `project.version` в `packages/00-core.yaml`. Проверка обновлений сравнивает её с манифестом релизов, поэтому:

- пока тестируете на своей плате, версию не меняйте;
- перед публикацией поднимите её (`1.0.0-beta.6`, …), соберите **`treadmill-public.yaml`** и скачайте её в панели ESPHome дважды (*Install → Manual download*): **OTA format** и **Factory format**. Затем опубликуйте оба файла:

```sh
PRE=1 tools/fw-release.sh 1.0.0-beta.6 path/to/firmware.ota.bin path/to/firmware.factory.bin "Короткое описание"
```

Скрипт создаёт pre-release `fw-v<версия>` с обоими образами, обновляет релиз `fw-manifest`, который опрашивают платы для обновления по Wi-Fi, и пересобирает [страницу установки по USB](https://samsonovss.github.io/ESPHome-Treadmill-FTMS/install/) с новым factory-образом. Номер опубликованной версии повторно не используйте.
