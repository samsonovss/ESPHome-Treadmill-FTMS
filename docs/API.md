# HTTP API

[Русская версия](API.ru.md) · [Back to README](../README.md) · [OpenAPI file](openapi.yaml)

The treadmill speaks plain HTTP on port 80. There are two layers:

1. **Standard ESPHome web server**: live state and control of every entity (speed, incline, buttons, settings).
2. **Treadmill endpoints**: workout archive, programs, routes, presets, and the [MCP server](MCP.md).

> [!WARNING]
> There is no authentication. Use the API only on a trusted home network. Control commands really move the belt.

## 1. Live state and control (ESPHome)

These follow the [ESPHome web server REST API](https://esphome.io/web-api/). Entities are addressed by their name, URL-encoded.

```sh
# live stream of every state change (Server-Sent Events)
curl -N http://treadmill.local/events

# read one entity
curl http://treadmill.local/sensor/Treadmill%20Speed%20Motor
curl http://treadmill.local/binary_sensor/Treadmill%20Usage

# press a button / set a number / pick an option
curl -X POST http://treadmill.local/button/<name>/press
curl -X POST 'http://treadmill.local/number/<name>/set?value=5'
curl -X POST 'http://treadmill.local/select/<name>/set?option=<option>'
```

Firmware update entity:

```sh
curl http://treadmill.local/update/Firmware%20Update          # state, current and latest version
curl -X POST http://treadmill.local/button/Check%20Firmware%20Update/press   # check GitHub now
curl -X POST -H 'Content-Length: 0' http://treadmill.local/update/Firmware%20Update/install  # belt must be stopped
```

The full list of entities is easiest to see in the `/events` stream or in Home Assistant.

## 2. Treadmill endpoints

Documented in [`openapi.yaml`](openapi.yaml) (open it in [Swagger Editor](https://editor.swagger.io) or any OpenAPI viewer):

| Endpoint | What it does |
|---|---|
| `GET /workouts/status` | Recorder state and storage usage |
| `GET /workouts/list?after=&limit=` | Page of saved workout IDs |
| `GET /workouts/file?id=&type=csv\|json` | Samples (~1 per second, CSV) or the summary (JSON) of one workout |
| `GET /api/workouts/trash/list` | Workouts in the trash |
| `POST /api/workouts/trash?id=` · `/api/workouts/restore?id=` | Move to trash / restore (asynchronous, poll `/api/workouts/job?job=`) |
| `POST /api/workouts/purge?id=` | Delete a workout from the trash for good (asynchronous, cannot be undone) |
| `GET /api/workouts/live?from=` | Samples of the current or last workout from memory, 1200 rows per page; works while recording |
| `GET /api/programs/list` · `GET /api/programs/file?id=` | Stored programs |
| `POST /api/programs/save` | Save a program: JSON body, `Content-Type: application/json`, same ID overwrites (belt stopped, asynchronous) |
| `POST /api/programs/delete?id=` | Delete a program (belt stopped) |
| `POST /api/storage/scan` · `GET /api/storage/scan` | Start a storage scan / read the result: file count, largest files and folders |
| `POST /mcp` | [MCP server](MCP.md) |

Rules shared by the archive and program endpoints:

- archive reads return `409` while a workout is being recorded or finalized: retry after stop;
- writes are allowed only while the belt is stopped;
- workout IDs look like `w0000000001`, program IDs like `p` + 16 hex digits.

Routes (`/api/routes/*`) and presets (`/api/presets`) are used by the web page as well. Their format may still change, so they are not described here yet.
