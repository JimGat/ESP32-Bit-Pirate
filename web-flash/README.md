# ESP32-Bit-Pirate — JARVIS AI Enabled Edition Web Flasher

Static firmware flasher for the JimGat fork of
[ESP32-Bit-Pirate](https://github.com/geo-tp/ESP32-Bit-Pirate).

Built on [ESP Web Tools](https://github.com/esphome/esp-web-tools) — the same library
used by ESPHome and Home Assistant. It flashes over WebUSB from any Chrome / Edge / Brave
desktop browser.

## What it flashes

The firmware binaries built by PlatformIO for each supported board, pulled out of
`.pio/build/<env>/` after a normal `pio run -e <env>`.

Currently shipping:

| Board | Source build env | Status |
|---|---|---|
| ESP32-S3 DevKit | `s3-devkit` | ✅ Ready |
| ESP32-S3 Super Mini | `s3-supermini` | ✅ Ready |
| M5Stack Cardputer Adv | `cardputer-adv` | ✅ Ready |
| M5Stack StickS3 | `m5stack-sticks3` | ✅ Ready |
| LILYGO T-Display S3 | `t-display-s3` | ✅ Ready |
| LILYGO T-Embed S3 | `t-embed-s3` | ✅ Ready |
| LILYGO T-Embed S3 CC1101 | `t-embed-s3-cc1101` | ✅ Ready |
| LILYGO T-Embed S3 CC1101+ | `t-embed-s3-cc1101plus` | ✅ Ready |
| Seeed XIAO ESP32-S3 | `xiao-esp32s3` | ✅ Ready |

## Persistent Wi-Fi auto-connect

This flasher ships the JARVIS AI Enabled Edition firmware. After a user connects from `mode wifi` with `connect <ssid> <password>`, the credentials are stored in ESP32 NVS and reused on later boots. A configured BitPirate therefore comes back on the LAN automatically so local agents can reach the Web CLI, `/ws` stream, and `/api/*` automation endpoints.

JARVIS AI Enabled Edition deltas:

- `/api/status` and `/api/command` REST endpoints for bounded automation commands.
- `/ws` remains the streaming transport for sniffers, captures, raw RF/audio, and other unbounded protocol output.
- Saved Wi-Fi auto-connect is the default after a successful `connect`.
- `saved` verifies the saved SSID without exposing the password.
- `forget` clears saved credentials and disables boot auto-connect.
- `serial-once` or a board/user-button double-click while Web UI is active starts USB Serial on the next boot only without erasing saved Wi-Fi.
- Holding BOOT during reset/power-up still enters the ESP32 ROM flashing path; the recovery double-click is only after firmware is already running.

## Remote UART / serial console

For target serial console wiring, use the board-specific UART RX/TX table in [`docs/REMOTE_UART.md`](../docs/REMOTE_UART.md). USB Serial setup/recovery uses native USB CDC and does not conflict with the target UART GPIOs used by Web UI `mode uart`.

## Local preview

```bash
cd web-flash
python3 -m http.server 8080
# open http://localhost:8080 in Chrome/Edge
```

> WebUSB does **not** work from `file://` URLs — you must serve over HTTP/HTTPS.

## Directory layout

```text
web-flash/
├── index.html                # Pirate-themed UI + ESP Web Tools component
├── boards/
│   └── s3-devkit/
│       ├── manifest.json     # Bin offsets for this board
│       ├── bootloader.bin
│       ├── partitions.bin
│       ├── boot_app0.bin
│       └── firmware.bin
└── README.md
```

## Adding a new board

1. Build the firmware:
   ```bash
   pio run -e <env>
   ```
2. Create `boards/<env>/` and copy the outputs:
   ```bash
   mkdir -p boards/<env>
   cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin boards/<env>/
   cp .pio/build/<env>/bootloader.bin boards/<env>/
   cp .pio/build/<env>/partitions.bin boards/<env>/
   cp .pio/build/<env>/firmware.bin boards/<env>/
   ```
3. Create `boards/<env>/manifest.json` with the board's flash offsets (see the
   `s3-devkit` example for the default ESP32 Arduino partition scheme).
4. Add the board to the `BOARDS` array in `index.html` and set `enabled: true`.

## Flash offsets reference

Default Arduino ESP32 partition layout (used unless `platformio.ini` overrides it):

| File            | Offset       | Hex      |
|-----------------|--------------|----------|
| bootloader.bin  | 0            | 0x0000   |
| partitions.bin  | 32768        | 0x8000   |
| boot_app0.bin   | 57344        | 0xE000   |
| firmware.bin    | 65536        | 0x10000  |

> Some boards use a custom partition scheme — check `partitions.csv` in the build
> output or run `pio run -e <env> -t bootloader` to confirm offsets.

## Hosting on GitHub Pages

GitHub Actions deploys the `web-flash/` directory from `main` using `.github/workflows/deploy-web-flash.yml`.

URL: `https://jimgat.github.io/ESP32-Bit-Pirate/`

## CI integration (future)

A GitHub Actions workflow can automate:

1. `pio run -e <env>` for each board on push / release tag.
2. Copy bins into `web-flash/boards/<env>/`.
3. Commit + deploy `web-flash/` to Pages.

That keeps the flasher and the firmware always in lock-step without any manual copying.

## Upstream compatibility

This flasher is fork-specific — it ships JimGat binaries.
The firmware itself remains fully compatible with upstream Arduino IDE and PlatformIO
builds, so any fixes we make can be submitted as clean upstream PRs without dragging
the flasher along.
