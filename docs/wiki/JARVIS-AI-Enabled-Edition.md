# JARVIS AI Enabled Edition

The JimGat fork adds LAN-first automation behavior for JARVIS and other local agents while keeping the BitPirate protocol tools recognizable.

Key differences:

- Public multi-board web flasher.
- `/api/status` and `/api/command` REST endpoints for bounded automation.
- `/ws` WebSocket remains the required transport for streaming protocol work.
- Successful `mode wifi` / `connect <ssid> <password>` stores Wi-Fi credentials in NVS.
- Saved Wi-Fi auto-connects on boot and starts Web UI/API on the LAN.
- `saved` shows saved SSID only; password is always redacted.
- `forget` clears saved Wi-Fi and disables boot auto-connect.
- `serial-once` or board/user-button double-click while Web UI is active starts USB Serial on the next boot only.

Holding BOOT during reset/power-up is still ESP32 ROM download/programming mode.
