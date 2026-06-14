# AVRDUDE WebAssembly artifacts

The application first looks for these vendored files:

- `wasm/avrdude.js`
- `wasm/avrdude.wasm`
- `wasm/avrdude.conf`
- `avrdude-worker.js`

These files are vendored from the published npm package
`@leaphy-robotics/avrdude-webassembly@1.7.1`. To refresh or restore them, run
from `avr-programmer/`:

```sh
node scripts/vendor-avrdude-wasm.mjs
```

The WebAssembly serial bridge uses `SharedArrayBuffer`. The server must return:

```text
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

Keep the third-party GPL notices in `../licenses/` when distributing the
vendored artifacts.
