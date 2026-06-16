const WEBSTLINK_MODULE_URL = "https://devanlai.github.io/webstlink/src/webstlink.js";
const ST_VENDOR_ID = 0x0483;
const STLINK_DEBUG_COMMAND = 0xf2;
const STLINK_GET_VERSION = 0xf1;
const STLINK_GET_TARGET_VOLTAGE = 0xf7;
const STLINK_DEBUG_READ_CORE_ID = 0x22;
const STLINK_DEBUG_API_V2_READ_IDCODES = 0x31;
const STLINK_DEBUG_API_V2_SET_SWD_FREQ = 0x43;
const STLINK_STATUS_OK = 0x80;
const STLINK_SWD_SPEEDS = [
  { label: "1.8 MHz", divisor: 1 },
  { label: "480 kHz", divisor: 7 },
  { label: "100 kHz", divisor: 40 },
];
const STLINK_FILTERS = [
  { vendorId: ST_VENDOR_ID, productId: 0x3748 }, // ST-LINK/V2
  { vendorId: ST_VENDOR_ID, productId: 0x374b }, // ST-LINK/V2-1
];

export class StlinkWebUsbError extends Error {
  constructor(message, cause = null) {
    super(message);
    this.name = "StlinkWebUsbError";
    this.cause = cause;
  }
}

export class StlinkWebUsbAdapter {
  constructor({ log = () => {}, moduleLoader = null, requestDevice = null } = {}) {
    this.log = log;
    this.moduleLoader = moduleLoader ?? (() => import(WEBSTLINK_MODULE_URL).then((module) => module.default));
    this.requestDevice = requestDevice ?? ((options) => navigator.usb.requestDevice(options));
    this.webstlink = null;
    this.usbDevice = null;
    this.target = null;
    this.device = null;
    this.flashSize = null;
    this.connected = false;
    this.status = null;
    this._modulePromise = null;
    this._operationProgress = null;
  }

  static isSupported() {
    return typeof navigator !== "undefined" && Boolean(navigator.usb);
  }

  async connect({ signal } = {}) {
    throwIfAborted(signal);
    if (!StlinkWebUsbAdapter.isSupported()) {
      throw new StlinkWebUsbError("WebUSB is unavailable in this browser.");
    }

    let device = null;
    let probe = null;
    let compatibilityState = null;
    try {
      device = await this.requestDevice({ filters: STLINK_FILTERS });
      throwIfAborted(signal);

      const WebStlink = await this.loadLibrary();
      const logger = createLogger(this.log, (event) => this.handleBargraphProgress(event));
      let lastAttachError = null;

      for (let index = 0; index < STLINK_SWD_SPEEDS.length; index += 1) {
        const speed = STLINK_SWD_SPEEDS[index];
        throwIfAborted(signal);
        if (index > 0) {
          this.log(`Retrying the SWD connection at ${speed.label}...`);
        }

        probe = new WebStlink(logger);
        const compatibility = createStlinkCompatibilityDevice(device, {
          swdDivisor: speed.divisor,
          swdSpeedLabel: speed.label,
          log: this.log,
        });
        compatibilityState = compatibility.state;

        try {
          await probe.attach(compatibility.device, logger);
          lastAttachError = null;
          break;
        } catch (error) {
          lastAttachError = enhanceStlinkAttachError(error, compatibilityState);
          probe = null;
          if (!shouldRetryStlinkAttach(lastAttachError) || index === STLINK_SWD_SPEEDS.length - 1) {
            throw lastAttachError;
          }
        }
      }

      if (!probe) {
        throw lastAttachError ?? new StlinkWebUsbError("The ST-Link probe could not enter SWD mode.");
      }

      throwIfAborted(signal);
      const target = await probe.detect_cpu([], null);
      let status = await probe.inspect_cpu();
      if (!status.debug) {
        await probe.set_debug_enable(true);
        status = await probe.inspect_cpu();
      }

      this.webstlink = probe;
      this.usbDevice = device;
      this.target = target;
      this.status = status;
      this.connected = true;
      this.updateDeviceInfo();
      this.log(`Connected through ${this.getProbeName()} to ${this.device.name}.`);
      return this.getDeviceInfo();
    } catch (error) {
      if (probe && probe !== this.webstlink) {
        try {
          await probe.detach();
        } catch {
          // Keep the original connection error.
        }
      }
      this.connected = false;
      this.webstlink = null;
      this.usbDevice = null;
      this.target = null;
      this.device = null;
      this.flashSize = null;
      this.status = null;
      throw normalizeStlinkError(enhanceStlinkAttachError(error, compatibilityState));
    }
  }

  async disconnect() {
    const probe = this.webstlink;
    this.connected = false;
    this.webstlink = null;
    this.target = null;
    this.device = null;
    this.flashSize = null;
    this.status = null;
    this.usbDevice = null;
    if (probe) {
      try {
        await probe.detach();
      } catch (error) {
        this.log(`ST-Link disconnect warning: ${messageOf(error)}`);
      }
    }
  }

  handleUsbDisconnect(device) {
    if (device !== this.usbDevice) return false;
    this.connected = false;
    this.webstlink = null;
    this.target = null;
    this.device = null;
    this.flashSize = null;
    this.status = null;
    this.usbDevice = null;
    return true;
  }

  async refreshInfo({ signal } = {}) {
    this.ensureConnected();
    throwIfAborted(signal);
    const target = await this.webstlink.detect_cpu([], null);
    this.target = target;
    this.status = await this.webstlink.inspect_cpu();
    this.updateDeviceInfo();
    return this.getDeviceInfo();
  }

  async readMemory(address, length, { progress = null, signal, chunkSize = 16 * 1024 } = {}) {
    this.ensureConnected();
    validateRange(address, length);
    await this.ensureHalted(signal);

    const output = new Uint8Array(length);
    let done = 0;
    while (done < length) {
      throwIfAborted(signal);
      const size = Math.min(chunkSize, length - done);
      const chunk = await this.webstlink.read_memory(address + done, size);
      output.set(toUint8Array(chunk), done);
      done += size;
      progress?.({ done, total: length });
    }
    return output;
  }

  async flash(address, bytes, { progress = null, signal } = {}) {
    this.ensureConnected();
    const data = toUint8Array(bytes);
    validateRange(address, data.length);
    throwIfAborted(signal);
    await this.ensureHalted(signal);
    progress?.({ done: 0, total: 100 });
    this._operationProgress = progress;
    try {
      await this.webstlink.flash(address, data);
      throwIfAborted(signal);
      progress?.({ done: 100, total: 100 });
    } finally {
      this._operationProgress = null;
    }
  }

  handleBargraphProgress({ message = "", percent = 0 } = {}) {
    if (!this._operationProgress) return;
    const normalized = Math.min(100, Math.max(0, Number(percent) || 0));
    const phase = String(message).toLowerCase();
    const mapped = phase.includes("eras")
      ? normalized * 0.2
      : phase.includes("writ") || phase.includes("flash")
        ? 20 + normalized * 0.8
        : normalized;
    this._operationProgress({ done: mapped, total: 100, stage: message });
  }

  async verifyMemory(address, expected, { progress = null, signal } = {}) {
    const bytes = toUint8Array(expected);
    const actual = await this.readMemory(address, bytes.length, { progress, signal });
    for (let index = 0; index < bytes.length; index += 1) {
      if (actual[index] !== bytes[index]) {
        throw new StlinkWebUsbError(
          `Verification failed at ${formatHex(address + index)}: expected 0x${hexByte(bytes[index])}, read 0x${hexByte(actual[index])}.`,
        );
      }
    }
  }

  async massErase({ signal } = {}) {
    this.ensureConnected();
    throwIfAborted(signal);
    await this.ensureHalted(signal);
    const eraseAll = this.webstlink?._driver?.flash_erase_all;
    if (typeof eraseAll !== "function") {
      throw new StlinkWebUsbError("Mass erase is not available for this STM32 target through the current ST-Link driver.");
    }
    await eraseAll.call(this.webstlink._driver);
    throwIfAborted(signal);
  }

  async eraseRange(address, length, { progress = null, signal } = {}) {
    this.ensureConnected();
    validateRange(address, length);
    // webstlink erases every page/sector overlapping the supplied image. An
    // all-0xFF image therefore performs a range erase without programming data.
    const erased = new Uint8Array(length);
    erased.fill(0xff);
    await this.flash(address, erased, { progress, signal });
  }

  async go(_address = null, { signal } = {}) {
    this.ensureConnected();
    throwIfAborted(signal);
    await this.webstlink.reset(false);
    throwIfAborted(signal);
    await this.webstlink.run();
    this.status = await this.webstlink.inspect_cpu();
  }

  getDeviceInfo() {
    const target = this.target;
    const probeVersion = this.webstlink?._stlink?.ver_str ?? "V2";
    const deviceId = target?.dev_id ?? null;
    const flashStart = target?.flash_start ?? 0x08000000;
    const flashSize = this.flashSize ?? 128 * 1024;
    return {
      connected: this.connected,
      transport: "stlink",
      deviceId,
      deviceIdText: deviceId == null ? "-" : `0x${Number(deviceId).toString(16).padStart(3, "0").toUpperCase()}`,
      name: this.device?.name ?? "Unknown STM32",
      family: this.device?.family ?? "STM32",
      bootloaderVersion: null,
      bootloaderVersionText: `ST-Link/${probeVersion}`,
      optionBytes: null,
      optionBytesText: this.status?.halted ? "SWD / halted" : "SWD / running",
      commands: [],
      commandNames: ["SWD", "READ_MEMORY", "FLASH", "ERASE", "RESET_RUN"],
      capabilities: ["SWD", target?.core, `${target?.sram_size ?? 0} KB SRAM`].filter(Boolean),
      flashStart,
      flashSize,
      flashSizeDetected: true,
      knownDevice: Boolean(target),
      geometry: this.device?.geometry ?? null,
      portInfo: {
        usbVendorId: this.usbDevice?.vendorId ?? ST_VENDOR_ID,
        usbProductId: this.usbDevice?.productId ?? null,
        productName: this.usbDevice?.productName ?? "ST-Link",
        serialNumber: this.usbDevice?.serialNumber ?? "",
      },
      core: target?.core ?? "-",
      sramSize: (target?.sram_size ?? 0) * 1024,
      targetVoltage: this.webstlink?._stlink?.target_voltage ?? null,
    };
  }

  updateDeviceInfo() {
    const target = this.target;
    const flashSize = Math.max(1, Number(target?.flash_size) || 128) * 1024;
    this.flashSize = flashSize;
    this.device = {
      deviceId: target?.dev_id ?? null,
      known: Boolean(target),
      name: target?.type ?? "Unknown STM32",
      family: inferFamily(target?.type),
      flashStart: target?.flash_start ?? 0x08000000,
      defaultFlashSize: flashSize,
      maxFlashSize: flashSize,
      geometry: deriveGeometry(this.webstlink?._mcus_by_devid?.erase_sizes, flashSize),
    };
  }

  async ensureHalted(signal) {
    throwIfAborted(signal);
    let status = await this.webstlink.inspect_cpu();
    if (!status.debug) {
      await this.webstlink.set_debug_enable(true);
      status = await this.webstlink.inspect_cpu();
    }
    if (!status.halted) {
      await this.webstlink.halt();
      status = await this.webstlink.inspect_cpu();
    }
    this.status = status;
  }

  ensureConnected() {
    if (!this.connected || !this.webstlink) {
      throw new StlinkWebUsbError("Connect an ST-Link probe first.");
    }
  }

  async loadLibrary() {
    if (!this._modulePromise) {
      this.log("Loading the ST-Link WebUSB driver...");
      this._modulePromise = Promise.resolve()
        .then(() => this.moduleLoader())
        .catch((error) => {
          this._modulePromise = null;
          throw new StlinkWebUsbError(
            "The ST-Link WebUSB driver could not be loaded. Check the Internet connection and reload the page.",
            error,
          );
        });
    }
    return this._modulePromise;
  }

  getProbeName() {
    const product = this.usbDevice?.productName || "ST-Link";
    const version = this.webstlink?._stlink?.ver_str;
    return version && !product.includes(version) ? `${product} (${version})` : product;
  }
}


/**
 * Compatibility wrapper for the older webstlink driver.
 *
 * Modern ST-Link/V2 firmware uses the API-v2 READ_IDCODES command (0x31),
 * which returns a 12-byte response with the core ID at offset 4. The upstream
 * webstlink driver still sends the API-v1 READCOREID command (0x22) and then
 * blindly reads four bytes. Several ST-Link/V2 dongles answer that legacy
 * command with only two bytes, which produces a DataView bounds exception.
 */
export function createStlinkCompatibilityDevice(
  usbDevice,
  { swdDivisor = 1, swdSpeedLabel = "1.8 MHz", log = () => {} } = {},
) {
  const state = {
    apiVersion: null,
    jtagVersion: null,
    targetVoltage: null,
    coreIdApi2Pending: false,
    coreIdCompatibilityUsed: false,
    lastCommand: new Uint8Array(0),
    swdDivisor,
    swdSpeedLabel,
  };

  const device = new Proxy(usbDevice, {
    get(target, property) {
      if (property === "transferOut") {
        return async (endpointNumber, data) => {
          const original = toUint8Array(data);
          const command = original.slice();
          state.lastCommand = original.slice();
          state.coreIdApi2Pending = false;

          if (
            command[0] === STLINK_DEBUG_COMMAND
            && command[1] === STLINK_DEBUG_API_V2_SET_SWD_FREQ
            && command.length >= 3
          ) {
            command[2] = swdDivisor;
            if (swdDivisor !== original[2]) {
              log(`Using a reduced SWD frequency (${swdSpeedLabel}).`);
            }
          }

          if (
            state.apiVersion === 2
            && command[0] === STLINK_DEBUG_COMMAND
            && command[1] === STLINK_DEBUG_READ_CORE_ID
          ) {
            command[1] = STLINK_DEBUG_API_V2_READ_IDCODES;
            state.coreIdApi2Pending = true;
          }

          return target.transferOut(endpointNumber, command);
        };
      }

      if (property === "transferIn") {
        return async (endpointNumber, length) => {
          if (state.coreIdApi2Pending) {
            state.coreIdApi2Pending = false;
            const result = await target.transferIn(endpointNumber, Math.max(12, length));
            const bytes = dataViewBytes(result?.data);
            if (result?.status !== "ok") return result;
            if (bytes.length < 8) {
              throw createCoreIdResponseError(bytes, state);
            }
            if (bytes[0] !== STLINK_STATUS_OK) {
              throw createCoreIdResponseError(bytes, state);
            }

            const coreId = bytes.slice(4, 8);
            if (isInvalidCoreId(coreId)) {
              throw createCoreIdResponseError(bytes, state);
            }

            if (!state.coreIdCompatibilityUsed) {
              log("Using the ST-Link API-v2 core-ID compatibility path.");
              state.coreIdCompatibilityUsed = true;
            }
            return {
              status: result.status,
              data: new DataView(coreId.buffer, coreId.byteOffset, coreId.byteLength),
            };
          }

          const result = await target.transferIn(endpointNumber, length);
          const bytes = dataViewBytes(result?.data);
          inspectCompatibilityResponse(state, bytes);
          return result;
        };
      }

      const value = Reflect.get(target, property, target);
      return typeof value === "function" ? value.bind(target) : value;
    },
  });

  return { device, state };
}

function inspectCompatibilityResponse(state, bytes) {
  const command = state.lastCommand;
  if (command[0] === STLINK_GET_VERSION && bytes.length >= 2) {
    const version = (bytes[0] << 8) | bytes[1];
    state.jtagVersion = (version >> 6) & 0x3f;
    state.apiVersion = state.jtagVersion > 11 ? 2 : 1;
  }

  if (command[0] === STLINK_GET_TARGET_VOLTAGE && bytes.length >= 8) {
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const a0 = view.getUint32(0, true);
    const a1 = view.getUint32(4, true);
    state.targetVoltage = a0 ? (2 * a1 * 1.2) / a0 : null;
  }
}

function dataViewBytes(view) {
  if (!view) return new Uint8Array(0);
  return new Uint8Array(view.buffer, view.byteOffset, view.byteLength);
}

function isInvalidCoreId(bytes) {
  if (bytes.length < 4) return true;
  const value = new DataView(bytes.buffer, bytes.byteOffset, 4).getUint32(0, true);
  return value === 0 || value === 0xffffffff;
}

function createCoreIdResponseError(bytes, state) {
  const response = bytes.length ? [...bytes].map(hexByte).join(" ") : "empty";
  const voltage = Number.isFinite(state?.targetVoltage)
    ? ` Target voltage: ${state.targetVoltage.toFixed(2)} V.`
    : "";
  return new StlinkWebUsbError(
    `The ST-Link entered SWD mode but the target did not return a valid core ID (response: ${response}).${voltage} Check SWDIO, SWCLK, GND, target power and optionally NRST.`,
  );
}

function enhanceStlinkAttachError(error, state) {
  if (error instanceof StlinkWebUsbError) return error;
  const message = messageOf(error);
  if (/offset is outside the bounds of the dataview/i.test(message)) {
    const voltage = Number.isFinite(state?.targetVoltage)
      ? ` Target voltage: ${state.targetVoltage.toFixed(2)} V.`
      : "";
    return new StlinkWebUsbError(
      `The ST-Link returned a truncated SWD core-ID response.${voltage} Check the SWD wiring and retry; the tool will also fall back to a lower SWD frequency.`,
      error,
    );
  }
  return error;
}

function shouldRetryStlinkAttach(error) {
  const message = messageOf(error);
  return /core.?id|swd|target|dataview|truncated|not connected/i.test(message);
}

function createLogger(log, onBargraph = null) {
  const write = (level, value) => {
    const message = messageOf(value);
    if (!message) return;
    log(level === "INFO" ? message : `${level}: ${message}`);
  };

  let graph = null;
  const readOptions = (value, fallbackMin = 0, fallbackMax = 100) => {
    if (value && typeof value === "object") {
      return {
        min: Number(value.value_min ?? value.min ?? fallbackMin),
        max: Number(value.value_max ?? value.max ?? fallbackMax),
      };
    }
    return { min: Number(value ?? fallbackMin), max: Number(fallbackMax) };
  };
  const readValue = (value) => Number(value && typeof value === "object" ? value.value : value);

  return {
    clear() {},
    debug(value) { write("DEBUG", value); },
    verbose(value) { write("DEBUG", value); },
    info(value) { write("INFO", value); },
    message(value) { write("INFO", value); },
    warning(value) { write("WARNING", value); },
    warn(value) { write("WARNING", value); },
    error(value) { write("ERROR", value); },
    bargraph_start(message, optionsOrMin = 0, max = 100) {
      const limits = readOptions(optionsOrMin, 0, max);
      graph = { message: String(message ?? ""), ...limits };
      onBargraph?.({ type: "start", message: graph.message, percent: 0 });
    },
    bargraph_update(value = 0, explicitPercent = null) {
      if (!graph) return;
      const current = readValue(value);
      const span = graph.max - graph.min;
      const percent = explicitPercent == null
        ? (span > 0 ? ((current - graph.min) / span) * 100 : 0)
        : Number(explicitPercent);
      onBargraph?.({
        type: "update",
        message: graph.message,
        percent: Math.min(100, Math.max(0, Number(percent) || 0)),
      });
    },
    bargraph_done() {
      if (!graph) return;
      onBargraph?.({ type: "done", message: graph.message, percent: 100 });
      graph = null;
    },
    set_verbose() {},
  };
}

function deriveGeometry(rawSizes, flashSize) {
  const sizes = Array.isArray(rawSizes)
    ? rawSizes.map(Number).filter((value) => Number.isInteger(value) && value > 0)
    : [];
  if (!sizes.length) return null;
  if (sizes.length === 1) return { type: "uniform", pageSize: sizes[0] };

  const expanded = [];
  let total = 0;
  let cursor = 0;
  while (total < flashSize && expanded.length < 4096) {
    const size = sizes[cursor % sizes.length];
    expanded.push(Math.min(size, flashSize - total));
    total += size;
    cursor += 1;
  }
  return total >= flashSize ? { type: "sectors", sizes: expanded } : null;
}

function inferFamily(type) {
  const match = String(type ?? "").toUpperCase().match(/STM32([A-Z]\d|[A-Z]{2})/);
  return match ? `STM32${match[1]}` : "STM32";
}

function validateRange(address, length) {
  if (!Number.isInteger(address) || address < 0 || address > 0xffffffff) {
    throw new StlinkWebUsbError("Memory address is invalid.");
  }
  if (!Number.isInteger(length) || length <= 0 || address + length - 1 > 0xffffffff) {
    throw new StlinkWebUsbError("Memory length is invalid.");
  }
}

function toUint8Array(value) {
  if (value instanceof Uint8Array) return value;
  if (value instanceof ArrayBuffer) return new Uint8Array(value);
  if (ArrayBuffer.isView(value)) return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  return Uint8Array.from(value ?? []);
}

function normalizeStlinkError(error) {
  if (error instanceof StlinkWebUsbError) return error;
  if (error?.name === "NotFoundError") {
    return new StlinkWebUsbError("No ST-Link probe was selected.", error);
  }
  if (error?.name === "SecurityError") {
    return new StlinkWebUsbError("WebUSB access was denied. Use HTTPS or localhost and allow access to the ST-Link probe.", error);
  }
  if (error?.name === "NetworkError" || /access|permission|claim/i.test(messageOf(error))) {
    return new StlinkWebUsbError("Chrome found the ST-Link but could not claim it. Close other ST-Link software and check the Linux USB permissions.", error);
  }
  return new StlinkWebUsbError(messageOf(error) || "ST-Link communication failed.", error);
}

function throwIfAborted(signal) {
  if (signal?.aborted) throw new DOMException("Operation cancelled.", "AbortError");
}

function messageOf(value) {
  return value instanceof Error ? value.message : String(value ?? "");
}

function formatHex(value) {
  return `0x${(Number(value) >>> 0).toString(16).padStart(8, "0").toUpperCase()}`;
}

function hexByte(value) {
  return (Number(value) & 0xff).toString(16).padStart(2, "0").toUpperCase();
}
