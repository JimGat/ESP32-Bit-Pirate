export class Stm32SerialError extends Error {
  constructor(message, cause) {
    super(message);
    this.name = "Stm32SerialError";
    this.cause = cause;
  }
}

export class Stm32SerialTimeoutError extends Stm32SerialError {
  constructor(message = "Timed out while waiting for the STM32 bootloader.") {
    super(message);
    this.name = "Stm32SerialTimeoutError";
  }
}

export class Stm32SerialTransport {
  constructor({ bufferSize = 65536 } = {}) {
    this.bufferSize = bufferSize;
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.readQueue = [];
    this.waiters = [];
    this.reading = false;
    this.readLoopPromise = null;
    this.streamHandler = null;
    this.openOptions = null;
  }

  static isSupported() {
    return typeof navigator !== "undefined" && "serial" in navigator;
  }

  async requestPort(filters = []) {
    if (!Stm32SerialTransport.isSupported()) {
      throw new Stm32SerialError("Web Serial is not available in this browser.");
    }
    this.port = await navigator.serial.requestPort(filters.length ? { filters } : undefined);
    return this.port;
  }

  async open(options = {}) {
    if (!this.port) {
      throw new Stm32SerialError("No serial port has been selected.");
    }

    if (this.reader || this.writer || this.port.readable || this.port.writable) {
      await this.close({ keepPort: true });
    }

    const normalized = {
      baudRate: options.baudRate ?? 115200,
      dataBits: options.dataBits ?? 8,
      stopBits: options.stopBits ?? 1,
      parity: options.parity ?? "even",
      bufferSize: options.bufferSize ?? this.bufferSize,
      flowControl: options.flowControl ?? "none",
    };

    try {
      await this.port.open(normalized);
      this.openOptions = normalized;
      this.writer = this.port.writable.getWriter();
      this.startReadLoop();
    } catch (error) {
      throw new Stm32SerialError("Unable to open the serial port.", error);
    }
  }

  async requestAndOpen(options = {}) {
    await this.requestPort(options.filters ?? []);
    await this.open(options);
    return this.port;
  }

  async reopen(options = {}) {
    if (!this.port) {
      throw new Stm32SerialError("No serial port is available to reopen.");
    }
    await this.close({ keepPort: true });
    await sleep(80);
    await this.open(options);
  }

  async close({ keepPort = true } = {}) {
    const port = this.port;
    this.reading = false;

    if (this.reader) {
      try {
        await this.reader.cancel();
      } catch {
        // The stream can already be closed by the browser or device.
      }
    }

    if (this.readLoopPromise) {
      try {
        await this.readLoopPromise;
      } catch {
        // Read errors are surfaced through pending waiters.
      }
    }

    if (this.reader) {
      try {
        this.reader.releaseLock();
      } catch {
        // Reader may already have released its lock.
      }
      this.reader = null;
    }

    if (this.writer) {
      try {
        this.writer.releaseLock();
      } catch {
        // Writer may already have released its lock.
      }
      this.writer = null;
    }

    if (port?.readable || port?.writable) {
      try {
        await port.close();
      } catch (error) {
        if (!isAlreadyClosedError(error)) {
          throw new Stm32SerialError("Unable to close the serial port cleanly.", error);
        }
      }
    }

    this.readQueue = [];
    this.rejectWaiters(new Stm32SerialError("Serial port closed."));
    this.openOptions = null;
    this.streamHandler = null;

    if (!keepPort) {
      this.port = null;
    }
  }

  async write(bytes) {
    if (!this.writer) {
      throw new Stm32SerialError("Serial writer is not available.");
    }
    const data = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    await this.writer.write(data);
  }

  async readByte(timeoutMs = 2000) {
    if (this.readQueue.length) {
      return this.readQueue.shift();
    }
    if (!this.reading) {
      throw new Stm32SerialError("Serial reader is not available.");
    }

    return new Promise((resolve, reject) => {
      const waiter = {
        resolve,
        reject,
        timeoutId: setTimeout(() => {
          this.waiters = this.waiters.filter((item) => item !== waiter);
          reject(new Stm32SerialTimeoutError());
        }, timeoutMs),
      };
      this.waiters.push(waiter);
    });
  }

  async readExact(length, timeoutMs = 2000) {
    if (!Number.isInteger(length) || length < 0) {
      throw new TypeError("readExact length must be a positive integer.");
    }
    const output = new Uint8Array(length);
    for (let index = 0; index < length; index += 1) {
      output[index] = await this.readByte(timeoutMs);
    }
    return output;
  }

  async readAvailable(settleMs = 20) {
    if (settleMs > 0) {
      await sleep(settleMs);
    }
    const bytes = new Uint8Array(this.readQueue);
    this.readQueue = [];
    return bytes;
  }

  async flushInput(settleMs = 30) {
    this.readQueue = [];
    if (settleMs > 0) {
      await sleep(settleMs);
      this.readQueue = [];
    }
  }

  async setSignals(signals) {
    if (!this.port) {
      throw new Stm32SerialError("No serial port is selected.");
    }
    try {
      await this.port.setSignals(signals);
    } catch (error) {
      throw new Stm32SerialError("The selected serial adapter does not expose controllable RTS/DTR signals.", error);
    }
  }

  setStreamHandler(handler) {
    this.streamHandler = typeof handler === "function" ? handler : null;
  }

  getInfo() {
    try {
      return this.port?.getInfo?.() ?? {};
    } catch {
      return {};
    }
  }

  startReadLoop() {
    if (this.reading || !this.port?.readable) {
      return;
    }
    this.reading = true;
    this.readLoopPromise = this.readLoop();
  }

  async readLoop() {
    try {
      this.reader = this.port.readable.getReader();
      while (this.reading) {
        const { value, done } = await this.reader.read();
        if (done) {
          break;
        }
        if (value?.length) {
          this.pushBytes(value);
        }
      }
    } catch (error) {
      if (this.reading) {
        this.rejectWaiters(new Stm32SerialError("Serial read failed.", error));
      }
    } finally {
      this.reading = false;
      try {
        this.reader?.releaseLock();
      } catch {
        // Reader may already be released.
      }
      this.reader = null;
    }
  }

  pushBytes(bytes) {
    const streamBytes = [];
    for (const byte of bytes) {
      const waiter = this.waiters.shift();
      if (waiter) {
        clearTimeout(waiter.timeoutId);
        waiter.resolve(byte);
      } else if (this.streamHandler) {
        streamBytes.push(byte);
      } else {
        this.readQueue.push(byte);
      }
    }

    if (streamBytes.length && this.streamHandler) {
      this.streamHandler(new Uint8Array(streamBytes));
    }
  }

  rejectWaiters(error) {
    for (const waiter of this.waiters.splice(0)) {
      clearTimeout(waiter.timeoutId);
      waiter.reject(error);
    }
  }
}

function isAlreadyClosedError(error) {
  const message = String(error?.message ?? error).toLowerCase();
  return message.includes("closed") || message.includes("not open");
}

export function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
