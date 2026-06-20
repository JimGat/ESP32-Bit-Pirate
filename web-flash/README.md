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

| Board         | Source build env | Status   |
|---------------|------------------|----------|
| ESP32-S3 DevKit | `s3-devkit`      | ✅ Ready |
| Cardputer     | `cardputer`      | planned  |
| M5StickC Plus3| `m5stack-sticks3`| planned  |
| LilyGO T-Embed| `t-embed-s3`     | planned  |
| M5Stamp S3    | `m5stamps3`      | planned  |

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

Enable Pages on the `feature/direct-network-api` branch, root `/web-flash`.

URL: `https://<org>.github.io/esp32-bit-pirate/web-flash/`

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
