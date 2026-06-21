# Direct Automation API

The JARVIS AI Enabled Edition adds a small HTTP API intended for AI agents and other direct automation clients. It keeps the existing human Web CLI intact and reuses the same terminal dispatcher path, so API commands behave like commands typed in the Web terminal.

The project is a PlatformIO / Arduino-framework firmware, not a native ESP-IDF app. The API is implemented with the existing ESP-IDF `esp_http_server` used by the Web UI.

## LAN availability and boot auto-connect

The API is only useful to local agents if the BitPirate returns to the LAN after power loss or reboot. Successful `mode wifi` / `connect <ssid> <password>` operations now persist credentials in ESP32 NVS. On subsequent boots, firmware checks NVS before presenting the terminal-mode selector:

- saved credentials present + network reachable → boot directly into Wi-Fi Client mode and start Web UI, `/ws`, and `/api/*` on the LAN IP;
- no saved credentials or connection failure → fall back to the normal terminal selection flow;
- `saved` shows the saved SSID and redacts password state;
- `serial-once` starts USB Serial on the next boot only while keeping saved Wi-Fi intact;
- a board/user-button double-click while Web UI mode is active triggers the same one-shot serial recovery;
- `forget` clears saved SSID/password and disables future boot auto-connect.

This is intentional for the AI-enabled build: JARVIS and other local agents need the GPIO/protocol tool to be discoverable after normal resets or power loss without a human reselecting Wi-Fi on the device. Physical ROM download mode is unchanged: holding BOOT during reset/power-up still enters the ESP32 programming path; the serial recovery double-click is used only after firmware is already running.

## Endpoints

### `GET /api/status`

Cheap liveness and state check.

Example response:

```json
{
  "ok": true,
  "api_version": 1,
  "device": "ESP32-Bit-Pirate",
  "firmware": "1.5",
  "uptime_ms": 123456,
  "mode": "I2C",
  "terminal_mode": "WiFi Web",
  "terminal_ip": "192.168.1.50",
  "ip": "192.168.1.50",
  "mac": "AA:BB:CC:DD:EE:FF",
  "heap_free": 123456,
  "heap_min_free": 100000,
  "ws_client_connected": false,
  "api_busy": false,
  "auth": "none"
}
```

### `POST /api/command`

Injects one terminal command into the same queue used by the Web CLI and returns bounded captured output.

Request:

```json
{
  "cmd": "mode i2c",
  "timeout_ms": 3000,
  "quiet_ms": 250,
  "max_bytes": 4096
}
```

Response:

```json
{
  "ok": true,
  "timeout": false,
  "duration_ms": 275,
  "mode": "I2C",
  "output": "Mode changed to I2C\nI2C> ",
  "truncated": false
}
```

If the command does not produce a quiet output window before `timeout_ms`, the endpoint returns HTTP `504 Gateway Timeout` with `ok=false` and `timeout=true`.

If another API command is already running, it returns HTTP `409 Conflict`.

## Authentication

By default, this matches the existing web UI posture and does not require auth.

For protected builds, define `BITPIRATE_API_TOKEN` at compile time as a C string macro. When set, `/api/status` and `/api/command` require an HTTP Authorization header using Bearer auth.

Do not commit real lab tokens into `platformio.ini`. Keep local token build flags in an ignored local environment or pass them via your CI/build environment.

## Serial mirror

Every API command is mirrored to USB serial for local operator visibility:

```text
[API RX] mode i2c
[API TX] bytes=25 timeout=false
```

This lets a local operator watch activity over serial while an AI client controls the device over HTTP/HTTPS/WSS through a LAN proxy or tunnel.

## Design notes

- Existing `/ws` Web CLI remains unchanged for humans.
- API commands reuse the Web terminal input queue rather than duplicating protocol-controller logic.
- Terminal output is captured in a bounded 8 KB ring buffer inside `WebSocketServer`.
- `config.max_uri_handlers` is raised to support the existing UI/LittleFS routes plus API routes.
- This is intentionally not MCP. MCP can be added later as an optional wrapper around this direct API for MCP-only clients.

## Typical AI-agent flow

1. `GET /api/status`
2. If alive and not busy, `POST /api/command` with a safe read-only command.
3. Use short `timeout_ms` and bounded `max_bytes` by default.
4. Escalate to destructive / RF / write operations only with explicit operator approval.
