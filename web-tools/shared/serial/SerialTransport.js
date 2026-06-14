import { SerialError, SerialTimeoutError, SerialUnsupportedError } from "./SerialErrors.js";

export class SerialTransport {
  constructor({ baudRate = 115200, bufferSize = 65536 } = {}) {
    this.baudRate = baudRate;
    this.bufferSize = bufferSize;
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.readQueue = [];
    this.waiters = [];
    this.reading = false;
    this.readLoopPromise = null;
  }

  static isSupported() {
    return "serial" in navigator;
  }

  async requestAndOpen() {
    if (!SerialTransport.isSupported()) {
      throw new SerialUnsupportedError();
    }

    this.port = await navigator.serial.requestPort();
    await this.port.open({
      baudRate: this.baudRate,
      dataBits: 8,
      stopBits: 1,
      parity: "none",
      bufferSize: this.bufferSize,
      flowControl: "none",
    });

    this.writer = this.port.writable.getWriter();

    try {
      await this.port.setSignals({ dataTerminalReady: true, requestToSend: false });
    } catch {
      // Some browser/driver combinations do not expose controllable signals.
    }

    this.startReadLoop();
  }

  async close() {
    const errors = [];

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
    this.rejectWaiters(new SerialError("Serial port closed."));

    if (errors.length) {
      throw new SerialError("Serial port closed with warnings.", errors[0]);
    }
  }

  async write(bytes) {
    if (!this.writer) {
      throw new SerialError("Serial writer is not available.");
    }

    await this.writer.write(bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes));
  }

  async readExact(length, timeoutMs = 2000) {
    const output = new Uint8Array(length);
    for (let offset = 0; offset < length; offset += 1) {
      output[offset] = await this.readByte(timeoutMs);
    }
    return output;
  }

  async readAvailable(settleMs = 20) {
    if (settleMs > 0) {
      await sleep(settleMs);
    }
    const bytes = [...this.readQueue];
    this.readQueue = [];
    return new Uint8Array(bytes);
  }

  async readByte(timeoutMs) {
    if (this.readQueue.length > 0) {
      return this.readQueue.shift();
    }

    if (!this.reading) {
      throw new SerialError("Serial reader is not available.");
    }

    return new Promise((resolve, reject) => {
      const waiter = {
        resolve,
        reject,
        timeoutId: window.setTimeout(() => {
          this.waiters = this.waiters.filter((item) => item !== waiter);
          reject(new SerialTimeoutError());
        }, timeoutMs),
      };
      this.waiters.push(waiter);
    });
  }

  async setSignals(signals) {
    if (!this.port) {
      return;
    }
    await this.port.setSignals(signals);
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
        if (value) {
          this.pushBytes(value);
        }
      }
    } catch (error) {
      if (this.reading) {
        this.rejectWaiters(new SerialError("Serial read failed.", error));
      }
    } finally {
      this.reading = false;
      try {
        this.reader?.releaseLock();
      } catch {
        // Reader can already be released after disconnect.
      }
      this.reader = null;
    }
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

function sleep(ms) {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}
