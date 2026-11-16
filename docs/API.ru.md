# HTTP API

[English version](API.md) · [Назад к README](../README.ru.md) · [Файл OpenAPI](openapi.yaml)

Дорожка работает по обычному HTTP на порту 80. API состоит из двух слоёв:

1. **Стандартный веб-сервер ESPHome**: состояние и управление всеми сущностями (скорость, наклон, кнопки, настройки).
2. **Эндпоинты дорожки**: архив тренировок, программы, маршруты, пресеты и [MCP-сервер](MCP.ru.md).

> [!WARNING]
> Авторизации нет. Используйте API только в доверенной домашней сети. Команды управления по-настоящему двигают полотно.

## 1. Состояние и управление (ESPHome)

Работает по [REST API веб-сервера ESPHome](https://esphome.io/web-api/). Сущности адресуются по имени в URL-кодировке.

```sh
# поток всех изменений состояния (Server-Sent Events)
curl -N http://treadmill.local/events

# прочитать одну сущность
curl http://treadmill.local/sensor/Treadmill%20Speed%20Motor
curl http://treadmill.local/binary_sensor/Treadmill%20Usage

# нажать кнопку / задать число / выбрать вариант
curl -X POST http://treadmill.local/button/<name>/press
curl -X POST 'http://treadmill.local/number/<name>/set?value=5'
curl -X POST 'http://treadmill.local/select/<name>/set?option=<option>'
```

Обновление прошивки:

```sh
curl http://treadmill.local/update/Firmware%20Update          # состояние, текущая и последняя версия
curl -X POST http://treadmill.local/button/Check%20Firmware%20Update/press   # проверить GitHub сейчас
curl -X POST -H 'Content-Length: 0' http://treadmill.local/update/Firmware%20Update/install  # полотно должно стоять
```

Полный список сущностей удобнее всего смотреть в потоке `/events` или в Home Assistant.

## 2. Эндпоинты дорожки

Описаны в [`openapi.yaml`](openapi.yaml) (открывается в [Swagger Editor](https://editor.swagger.io) или любом просмотрщике OpenAPI):

| Эндпоинт | Что делает |
|---|---|
| `GET /workouts/status` | Состояние записи и заполненность хранилища |
| `GET /workouts/list?after=&limit=` | Страница ID сохранённых тренировок |
| `GET /workouts/file?id=&type=csv\|json` | Отсчёты (~1 в секунду, CSV) или итоги (JSON) одной тренировки |
| `GET /api/workouts/trash/list` | Тренировки в корзине |
| `POST /api/workouts/trash?id=` · `/api/workouts/restore?id=` | В корзину / восстановить (асинхронно, статус в `/api/workouts/job?job=`) |
| `POST /api/workouts/purge?id=` | Удалить тренировку из корзины насовсем (асинхронно, без возврата) |
| `GET /api/workouts/live?from=` | Отсчёты текущей или последней тренировки из памяти, по 1200 строк; работает и во время записи |
| `GET /api/programs/list` · `GET /api/programs/file?id=` | Сохранённые программы |
| `POST /api/programs/save` | Сохранить программу: JSON в теле, `Content-Type: application/json`, тот же ID перезаписывается (полотно стоит, асинхронно) |
| `POST /api/programs/delete?id=` | Удалить программу (полотно стоит) |
| `POST /api/storage/scan` · `GET /api/storage/scan` | Запустить проверку хранилища / прочитать результат: число файлов, самые большие файлы и папки |
| `POST /mcp` | [MCP-сервер](MCP.ru.md) |

Общие правила для архива и программ:

- чтение архива возвращает `409`, пока тренировка записывается или завершается: повторите после остановки;
- запись разрешена только при остановленном полотне;
- ID тренировок выглядят как `w0000000001`, ID программ — `p` + 16 шестнадцатеричных цифр.

Маршруты (`/api/routes/*`) и пресеты (`/api/presets`) тоже использует веб-страница. Их формат ещё может меняться, поэтому здесь они пока не описаны.

## Чтение тренировок с закреплённым владельцем

Агент сначала читает `GET /api/profiles`: нужны `pending=false`, `active` и
соответствующий элемент `items` (применённые сохранённые настройки). Компактный
вариант MCP — `get_active_profile`, см. [порядок работы](MCP.ru.md).
`GET /workouts/list?owner=p1` и `/workouts/file?id=w0000000001&type=json&owner=p1`
(также `type=csv`) возвращают только записи этого применённого пользователя.
Несовпадение/неготовность/ожидающий профиль: 409 `owner_changed`; чужой файл: 404.
Список содержит `owner` и `context`; продолжайте по `next` даже при пустом items.
Неизвестный/неназначенный владелец исключается. **Без owner** сохранён прежний
общий доступ UI и миграции; `/api/workouts/assign` не изменён. Фильтрация корзины
не поддерживается (400). `/api/workouts/live` добавляет записанного владельца
`profile.id/name` либо null, не текущего выбранного пользователя. Это не авторизация.

### Персональные маршруты

Все GET/POST `/api/routes/list`, `/api/routes/file`, `/api/routes/save`, `/api/routes/delete`, `/api/routes/act` и `/api/route/active` требуют `X-Library-Owner` с ID применённого профиля из `/api/profiles`; отсутствующий/устаревший владелец — 409 `owner_changed`. Форматы и ID сохранены, пространство файлов — `personal-routes/<owner>/`. JSON (геометрия, детализация и остальные поля) и Nextion `.act` сохраняются вместе. Запись — только при остановке и свободном архиве.

Старый каталог `/routes` переносится атомарно исходному владельцу p1 (иначе первому постоянному профилю), с общей устойчивой привязкой библиотек и побайтовыми копиями `legacy-backup-routes`. Конфликт назначения блокирует перенос. Старые браузерные кэши не импортируются и не удаляются. Избранное и маркеры ACT персональны; публичный Zwift, калибровка и слои карты общие. При смене пользователя старый временный Custom route очищается и снимается с выбора, другой маршрут автоматически не включается. Nextion обновляет список и отклоняет устаревшую строку.
