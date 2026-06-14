# Shared Web Tool Modules

Small browser-only helpers shared by static ESP32 Bit Pirate web tools.

- `serial/SerialTransport.js`: byte-oriented Web Serial transport for protocol tools such as the SPI flash programmer.
- `serial/SerialErrors.js`: common Web Serial error types.
- `files/download.js`: binary download helper.

The Web Serial Terminal currently keeps its xterm-specific connection wrapper after being moved into `web-tools/web-serial-terminal/`. Future cleanup can adapt it to these shared primitives without changing the public tool URL.
