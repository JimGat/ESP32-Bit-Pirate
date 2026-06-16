const ESPTOOL_MODULE_URL = "https://unpkg.com/esptool-js@0.6.0/bundle.js";

export class EspToolAdapter {
  constructor({ log, onDeviceLost } = {}) {
    this.log = log ?? (() => {});
    this.onDeviceLost = onDeviceLost ?? (() => {});
    this.module = null;
    this.transport = null;
    this.loader = null;
    this.port = null;
    this.info = null;
  }

  async connect({ baudRate = 921600 } = {}) {
    if (!("serial" in navigator)) {
      throw new Error("Web Serial is not available in this browser.");
    }

    const { ESPLoader, Transport } = await this.loadModule();
    this.port = await navigator.serial.requestPort();
    this.transport = new Transport(this.port, false);
    this.transport.setDeviceLostCallback?.(() => this.onDeviceLost());
    this.loader = new ESPLoader({
      transport: this.transport,
      baudrate: baudRate,
      terminal: this.createTerminal(),
      debugLogging: false,
    });

    // ESPLoader.main() already switches from the ROM baud rate to the
    // configured baud rate after starting the stub. Calling changeBaud()
    // again can desynchronize some classic ESP32/PICO USB-UART bridges.
    const chipName = await this.loader.main("default_reset");
    const flashId = await this.safe(() => this.loader.readFlashId());
    const flashSize = await this.safe(() => this.loader.detectFlashSize());
    const mac = await this.safe(() => this.loader.chip?.readMac?.(this.loader));

    this.info = {
      chipName: chipName || this.loader.chip?.CHIP_NAME || "ESP",
      macAddress: formatMac(mac),
      flashId: typeof flashId === "number" ? `0x${flashId.toString(16).padStart(6, "0")}` : "-",
      flashSize: typeof flashSize === "string" ? flashSize : "-",
    };
    return this.info;
  }

  async disconnect() {
    try {
      await this.transport?.disconnect();
    } finally {
      this.transport = null;
      this.loader = null;
      this.port = null;
      this.info = null;
    }
  }

  async writeFlash(files, options = {}) {
    this.requireLoader();
    const flashSize = await this.resolveFlashSize(options.flashSize);
    await this.loader.writeFlash({
      fileArray: files.map((file) => ({ data: file.bytes, address: file.address })),
      flashMode: "keep",
      flashFreq: "keep",
      flashSize,
      eraseAll: Boolean(options.eraseAll),
      compress: true,
      reportProgress: options.onProgress,
    });

    if (options.verify) {
      for (let index = 0; index < files.length; index += 1) {
        const file = files[index];
        const deviceMd5 = await this.loader.flashMd5sum(file.address, file.bytes.length);
        const localMd5 = await md5Hex(file.bytes);
        options.onVerify?.(index, deviceMd5, localMd5);
        if (normalizeMd5(deviceMd5) !== localMd5) {
          throw new Error(`Verify failed for ${file.name}: device ${deviceMd5}, local ${localMd5}.`);
        }
      }
    }

    await this.loader.after(options.reboot ? "hard_reset" : "no_reset");
  }

  async resolveFlashSize(value) {
    if (value && value !== "detect") {
      return value;
    }
    const detected = this.info?.flashSize || await this.safe(() => this.loader.detectFlashSize());
    const normalized = normalizeFlashSizeLabel(detected);
    if (normalized) {
      this.log(`Using detected flash size for write: ${normalized}.`);
      return normalized;
    }
    return "detect";
  }

  async readFlash({ start, size, onProgress }) {
    this.requireLoader();
    return this.loader.readFlash(start, size, (packet, progress, totalSize) => {
      onProgress?.({ chunk: packet, done: progress, total: totalSize });
    });
  }

  async eraseFlash({ reboot = true } = {}) {
    this.requireLoader();
    const result = await this.loader.eraseFlash();
    await this.loader.after(reboot ? "hard_reset" : "no_reset");
    return result;
  }

  requireLoader() {
    if (!this.loader) {
      throw new Error("Connect to an ESP chip first.");
    }
  }

  async loadModule() {
    if (!this.module) {
      this.log("Loading esptool-js runtime...");
      this.module = await import(ESPTOOL_MODULE_URL);
      this.log("esptool-js runtime loaded.");
    }
    return this.module;
  }

  createTerminal() {
    return {
      clean: () => {},
      writeLine: (line) => this.log(line),
      write: (text) => this.log(text),
    };
  }

  async safe(task) {
    try {
      return await task();
    } catch (error) {
      this.log(`Info read warning: ${error instanceof Error ? error.message : String(error)}`);
      return null;
    }
  }
}

export function flashSizeToBytes(value) {
  if (!value || value === "-" || value === "detect") {
    return null;
  }
  const match = String(value).match(/^(\d+(?:\.\d+)?)\s*(KB|MB|K|M)?$/i);
  if (!match) {
    return null;
  }
  const amount = Number(match[1]);
  const unit = (match[2] || "B").toUpperCase();
  if (unit === "MB" || unit === "M") return Math.round(amount * 1024 * 1024);
  if (unit === "KB" || unit === "K") return Math.round(amount * 1024);
  return amount;
}

function normalizeFlashSizeLabel(value) {
  const bytes = flashSizeToBytes(value);
  if (!bytes) {
    return null;
  }
  const mb = bytes / 1024 / 1024;
  return Number.isInteger(mb) ? `${mb}MB` : null;
}

function formatMac(value) {
  if (!value) {
    return "-";
  }
  if (typeof value === "string") {
    return value;
  }
  if (Array.isArray(value) || value instanceof Uint8Array) {
    return Array.from(value).map((byte) => byte.toString(16).padStart(2, "0")).join(":");
  }
  return String(value);
}

async function md5Hex(bytes) {
  const words = [];
  const length = bytes.length;
  for (let index = 0; index < length; index += 1) {
    words[index >> 2] |= bytes[index] << ((index % 4) * 8);
  }
  words[length >> 2] |= 0x80 << ((length % 4) * 8);
  words[(((length + 8) >> 6) << 4) + 14] = length * 8;

  let a = 0x67452301;
  let b = 0xefcdab89;
  let c = 0x98badcfe;
  let d = 0x10325476;

  for (let i = 0; i < words.length; i += 16) {
    const aa = a;
    const bb = b;
    const cc = c;
    const dd = d;

    a = ff(a, b, c, d, words[i + 0], 7, -680876936);
    d = ff(d, a, b, c, words[i + 1], 12, -389564586);
    c = ff(c, d, a, b, words[i + 2], 17, 606105819);
    b = ff(b, c, d, a, words[i + 3], 22, -1044525330);
    a = ff(a, b, c, d, words[i + 4], 7, -176418897);
    d = ff(d, a, b, c, words[i + 5], 12, 1200080426);
    c = ff(c, d, a, b, words[i + 6], 17, -1473231341);
    b = ff(b, c, d, a, words[i + 7], 22, -45705983);
    a = ff(a, b, c, d, words[i + 8], 7, 1770035416);
    d = ff(d, a, b, c, words[i + 9], 12, -1958414417);
    c = ff(c, d, a, b, words[i + 10], 17, -42063);
    b = ff(b, c, d, a, words[i + 11], 22, -1990404162);
    a = ff(a, b, c, d, words[i + 12], 7, 1804603682);
    d = ff(d, a, b, c, words[i + 13], 12, -40341101);
    c = ff(c, d, a, b, words[i + 14], 17, -1502002290);
    b = ff(b, c, d, a, words[i + 15], 22, 1236535329);

    a = gg(a, b, c, d, words[i + 1], 5, -165796510);
    d = gg(d, a, b, c, words[i + 6], 9, -1069501632);
    c = gg(c, d, a, b, words[i + 11], 14, 643717713);
    b = gg(b, c, d, a, words[i + 0], 20, -373897302);
    a = gg(a, b, c, d, words[i + 5], 5, -701558691);
    d = gg(d, a, b, c, words[i + 10], 9, 38016083);
    c = gg(c, d, a, b, words[i + 15], 14, -660478335);
    b = gg(b, c, d, a, words[i + 4], 20, -405537848);
    a = gg(a, b, c, d, words[i + 9], 5, 568446438);
    d = gg(d, a, b, c, words[i + 14], 9, -1019803690);
    c = gg(c, d, a, b, words[i + 3], 14, -187363961);
    b = gg(b, c, d, a, words[i + 8], 20, 1163531501);
    a = gg(a, b, c, d, words[i + 13], 5, -1444681467);
    d = gg(d, a, b, c, words[i + 2], 9, -51403784);
    c = gg(c, d, a, b, words[i + 7], 14, 1735328473);
    b = gg(b, c, d, a, words[i + 12], 20, -1926607734);

    a = hh(a, b, c, d, words[i + 5], 4, -378558);
    d = hh(d, a, b, c, words[i + 8], 11, -2022574463);
    c = hh(c, d, a, b, words[i + 11], 16, 1839030562);
    b = hh(b, c, d, a, words[i + 14], 23, -35309556);
    a = hh(a, b, c, d, words[i + 1], 4, -1530992060);
    d = hh(d, a, b, c, words[i + 4], 11, 1272893353);
    c = hh(c, d, a, b, words[i + 7], 16, -155497632);
    b = hh(b, c, d, a, words[i + 10], 23, -1094730640);
    a = hh(a, b, c, d, words[i + 13], 4, 681279174);
    d = hh(d, a, b, c, words[i + 0], 11, -358537222);
    c = hh(c, d, a, b, words[i + 3], 16, -722521979);
    b = hh(b, c, d, a, words[i + 6], 23, 76029189);
    a = hh(a, b, c, d, words[i + 9], 4, -640364487);
    d = hh(d, a, b, c, words[i + 12], 11, -421815835);
    c = hh(c, d, a, b, words[i + 15], 16, 530742520);
    b = hh(b, c, d, a, words[i + 2], 23, -995338651);

    a = ii(a, b, c, d, words[i + 0], 6, -198630844);
    d = ii(d, a, b, c, words[i + 7], 10, 1126891415);
    c = ii(c, d, a, b, words[i + 14], 15, -1416354905);
    b = ii(b, c, d, a, words[i + 5], 21, -57434055);
    a = ii(a, b, c, d, words[i + 12], 6, 1700485571);
    d = ii(d, a, b, c, words[i + 3], 10, -1894986606);
    c = ii(c, d, a, b, words[i + 10], 15, -1051523);
    b = ii(b, c, d, a, words[i + 1], 21, -2054922799);
    a = ii(a, b, c, d, words[i + 8], 6, 1873313359);
    d = ii(d, a, b, c, words[i + 15], 10, -30611744);
    c = ii(c, d, a, b, words[i + 6], 15, -1560198380);
    b = ii(b, c, d, a, words[i + 13], 21, 1309151649);
    a = ii(a, b, c, d, words[i + 4], 6, -145523070);
    d = ii(d, a, b, c, words[i + 11], 10, -1120210379);
    c = ii(c, d, a, b, words[i + 2], 15, 718787259);
    b = ii(b, c, d, a, words[i + 9], 21, -343485551);

    a = add32(a, aa);
    b = add32(b, bb);
    c = add32(c, cc);
    d = add32(d, dd);
  }

  return [a, b, c, d].map(toHexLe).join("");
}

function normalizeMd5(value) {
  return String(value ?? "").toLowerCase().replace(/[^a-f0-9]/g, "").slice(-32);
}

function cmn(q, a, b, x, s, t) {
  return add32(rotateLeft(add32(add32(a, q), add32(x || 0, t)), s), b);
}

function ff(a, b, c, d, x, s, t) {
  return cmn((b & c) | (~b & d), a, b, x, s, t);
}

function gg(a, b, c, d, x, s, t) {
  return cmn((b & d) | (c & ~d), a, b, x, s, t);
}

function hh(a, b, c, d, x, s, t) {
  return cmn(b ^ c ^ d, a, b, x, s, t);
}

function ii(a, b, c, d, x, s, t) {
  return cmn(c ^ (b | ~d), a, b, x, s, t);
}

function rotateLeft(value, count) {
  return (value << count) | (value >>> (32 - count));
}

function add32(a, b) {
  return (a + b) | 0;
}

function toHexLe(value) {
  let output = "";
  for (let index = 0; index < 4; index += 1) {
    output += ((value >> (index * 8)) & 0xff).toString(16).padStart(2, "0");
  }
  return output;
}
