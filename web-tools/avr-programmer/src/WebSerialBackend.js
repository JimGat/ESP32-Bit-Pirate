export class WebSerialBackend {
  constructor({ log = () => {}, debugBytes = false } = {}) {
    this.log = log;
    this.debugBytes = debugBytes;
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.readQueue = [];
    this.waiters = [];
    this.readLoopPromise = null;
    this.writeQueue = Promise.resolve();
    this.closed = true;
  }

  static isSupported() {
    return "serial" in navigator;
  }

  async requestPort() {
    if (!WebSerialBackend.isSupported()) {
      throw new Error("Web Serial is not available in this browser.");
    }
    this.port = await navigator.serial.requestPort();
    this.log("Web Serial port selected.");
    return this.port;
  }

  async open(options = {}) {
    if (!this.port) {
      await this.requestPort();
    }

    const serialOptions = {
      baudRate: options.baudRate ?? 115200,
      dataBits: options.dataBits ?? 8,
      stopBits: options.stopBits ?? 1,
      parity: options.parity ?? "none",
      bufferSize: options.bufferSize ?? 65536,
      flowControl: options.flowControl ?? "none",
    };

    if (!this.port.readable || !this.port.writable) {
      await this.port.open(serialOptions);
    }

    this.closed = false;
    this.writer = this.port.writable.getWriter();
    this.reader = this.port.readable.getReader();
    this.readLoopPromise = this.readLoop();
    this.log(`Serial opened at ${serialOptions.baudRate} baud.`);
  }

  async write(buffer) {
    const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
    if (!this.writer) {
      throw new Error("Serial writer is not available.");
    }

    this.writeQueue = this.writeQueue.then(async () => {
      await this.writer.write(bytes);
      if (this.debugBytes) {
        this.log(`TX ${bytes.length} byte(s): ${formatBytes(bytes)}`);
      }
    });

    return this.writeQueue;
  }

  async read(length, timeout = 1000) {
    const output = new Uint8Array(length);
    for (let index = 0; index < length; index += 1) {
      output[index] = await this.readByte(timeout);
    }
    if (this.debugBytes) {
      this.log(`RX ${output.length} byte(s): ${formatBytes(output)}`);
    }
    return output;
  }

  async flushInput(settleMs = 0) {
    if (settleMs > 0) {
      await sleep(settleMs);
    }
    const flushed = this.readQueue.length;
    this.readQueue = [];
    this.log(`Serial input flushed (${flushed} buffered byte(s)).`);
  }

  async flushOutput() {
    await this.writeQueue;
    this.log("Serial output queue drained.");
  }

  async setSignals(signals) {
    if (!this.port?.setSignals) {
      return;
    }
    await this.port.setSignals(signals);
    this.log(`Serial signals updated: ${JSON.stringify(signals)}.`);
  }

  async close() {
    const errors = [];
    this.closed = true;
    this.rejectWaiters(new Error("Serial port closed."));

    if (this.reader) {
      try {
        await this.reader.cancel();
      } catch (error) {
        errors.push(error);
      }
      try {
        this.reader.releaseLock();
      } catch (error) {
        errors.push(error);
      }
      this.reader = null;
    }

    if (this.writer) {
      try {
        await this.writeQueue;
        await this.writer.close();
      } catch (error) {
        errors.push(error);
      }
      try {
        this.writer.releaseLock();
      } catch (error) {
        errors.push(error);
      }
      this.writer = null;
    }

    if (this.port) {
      try {
        await this.port.close();
      } catch (error) {
        errors.push(error);
      }
      this.port = null;
    }

    this.readQueue = [];
    this.log("Serial closed.");
    if (errors.length) {
      throw errors[0];
    }
  }

  async readLoop() {
    try {
      while (!this.closed) {
        const { value, done } = await this.reader.read();
        if (done) {
          break;
        }
        if (value) {
          this.pushBytes(value);
        }
      }
    } catch (error) {
      if (!this.closed) {
        this.rejectWaiters(error);
      }
    }
  }

  async readByte(timeout) {
    if (this.readQueue.length) {
      return this.readQueue.shift();
    }
    if (!this.reader) {
      throw new Error("Serial reader is not available.");
    }

    return new Promise((resolve, reject) => {
      const waiter = {
        resolve,
        reject,
        timeoutId: window.setTimeout(() => {
          this.waiters = this.waiters.filter((item) => item !== waiter);
          reject(new Error(`Serial read timeout after ${timeout} ms.`));
        }, timeout),
      };
      this.waiters.push(waiter);
    });
  }

  pushBytes(bytes) {
    for (const byte of bytes) {
      const waiter = this.waiters.shift();
      if (waiter) {
        window.clearTimeout(waiter.timeoutId);
        waiter.resolve(byte);
      } else {
        this.readQueue.push(byte);
      }
    }
  }

  rejectWaiters(error) {
    for (const waiter of this.waiters.splice(0)) {
      window.clearTimeout(waiter.timeoutId);
      waiter.reject(error);
    }
  }
}

function formatBytes(bytes) {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join(" ");
}

function sleep(ms) {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}
