# MCP server: AI agents

[Back to README](../README.md)

The treadmill runs a small [Model Context Protocol](https://modelcontextprotocol.io) server at `http://<treadmill>/mcp`. Claude and other MCP clients can read your workouts and store programs for you.

**Safety by design:** no tool can start a workout or move the belt. `save_program` only stores a validated program file; you start it yourself on the treadmill or on the web page. History tools and program writes are refused while a workout is running.

> [!WARNING]
> There is no authentication. Expose `/mcp` only on a trusted home network.

## Connect

*Settings → Agents (MCP)* on the treadmill's web page shows these snippets with your board's address filled in.

**Claude Code:**

```sh
claude mcp add --transport http treadmill http://treadmill.local/mcp
```

**Claude Desktop and other clients that only speak stdio** (through [`mcp-remote`](https://www.npmjs.com/package/mcp-remote)):

```json
{
  "mcpServers": {
    "treadmill": {
      "command": "npx",
      "args": ["-y", "mcp-remote", "http://treadmill.local/mcp", "--allow-http"]
    }
  }
}
```

Use the IP address instead of `treadmill.local` if your computer does not resolve `.local` names.

## Tools

| Tool | What it does |
|---|---|
| `get_status` | Recorder state (`idle` / `recording` / `finalizing`) and storage usage |
| `get_live` | Latest sample of the current or last workout: heart rate, speed, incline, distance, calories, zone |
| `list_workouts` | Page of saved workout IDs (`after`, `limit` ≤ 20); continue with `next` |
| `get_workout_summary` | Summary of one workout: distance, time, zones, load (`trimp`), 5-minute blocks |
| `list_programs` | IDs of programs stored on the board |
| `get_program` | Full JSON of one program |
| `save_program` | Store a program (same ID overwrites); only while stopped |
| `delete_program` | Delete a program; only while stopped; cannot be undone |

The `save_program` description that the server sends to the agent contains the full program format, the limits, and heart-rate zone guidance, so agents can plan sensible sessions without extra instructions.

## Program format (short)

```json
{
  "version": 1,
  "id": "p0123456789abcdef",
  "name": "Tempo 30",
  "schema_units": {"duration": "s", "speed": "km/h", "incline": "level"},
  "blocks": [
    {"repeat": 1, "steps": [{"duration": 300, "zone": 1, "incline": 0}]},
    {"repeat": 4, "steps": [{"duration": 180, "speed": 10.0, "incline": 2},
                            {"duration": 120, "speed": 7.0, "incline": 0}]},
    {"repeat": 1, "steps": [{"duration": 300, "zone": 1, "incline": 0}]}
  ]
}
```

- `id`: `p` + 16 lowercase hex digits;
- a step has `speed` (km/h) **or** `zone` (1-5: the treadmill's heart-rate regulator then sets the speed);
- optional `goal`, `plan` and `day` (`YYYY-MM-DD`) group programs into a weekly plan on the page;
- limits: up to 64 blocks × 64 steps, 4 hours in total, 8 KB; speed and incline must lie on the board's min/max/step grid. To be runnable from the page, keep it to about 25 steps after expanding repeats.

## Protocol

JSON-RPC 2.0 over HTTP `POST /mcp`, stateless (no sessions, no SSE). Methods: `initialize`, `ping`, `tools/list`, `tools/call`. Protocol versions `2024-11-05`, `2025-03-26` and `2025-06-18`. `GET /mcp` returns 405.

```sh
curl -s http://treadmill.local/mcp -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"get_status","arguments":{}}}'
```
