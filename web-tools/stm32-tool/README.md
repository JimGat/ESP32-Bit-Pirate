# STM32 Web Tool

Browser-based STM32 programmer with two connection modes:

- **UART Bootloader** through Web Serial and the factory ROM bootloader.
- **ST-Link** through WebUSB and SWD.

## Implemented

### UART Bootloader

- STM32 bootloader serial format (`8E1`)
- Manual BOOT0/RESET workflow
- Optional configurable RTS/DTR boot sequence
- Synchronization, command discovery and Device ID detection
- BIN and Intel HEX loading
- Required-page/sector erase or full-chip erase
- Chunked writing, readback verification and flash backup
- `GO` command and reusable `8N1` serial console

### ST-Link WebUSB

- ST-LINK/V2 (`0483:3748`) and ST-LINK/V2-1 (`0483:374b`) selection
- SWD target identification, core, Device ID, flash and SRAM information
- Flash reading and backup
- Firmware programming with affected-sector erase and verification
- Full-chip erase and range erase
- Target reset and run after programming
- USB disconnect and permission error handling
- ST-Link API-v2 core-ID compatibility (`READ_IDCODES`) with automatic SWD fallback at 1.8 MHz, 480 kHz and 100 kHz

## Structure

- `src/Stm32Bootloader.js` — AN3155 UART protocol engine
- `src/Stm32SerialTransport.js` — Web Serial transport and RTS/DTR control
- `src/StlinkWebUsbAdapter.js` — ST-Link/WebUSB integration
- `src/Stm32DeviceDatabase.js` — conservative UART device and erase-layout database
- `src/IntelHex.js` — Intel HEX parser
- `src/App.js` — shared application workflow and UI state

## Important limitations

The UART bootloader interface, UART pins and erase geometry vary between STM32 families. AN2606 and the target datasheet remain the source of truth.

The ST-Link mode loads the MIT-licensed `devanlai/webstlink` ES module on demand. Its upstream flash support is older and has mainly been tested with STM32F1 targets. Always test on non-critical hardware before relying on it for a specific STM32 family.

The serial console is available only in UART mode. A Virtual COM Port exposed by some ST-LINK/V2-1 or STLINK-V3 devices is a separate UART interface; SWD programming still uses WebUSB.

## Tests

```sh
node tests/run-tests.mjs
```
