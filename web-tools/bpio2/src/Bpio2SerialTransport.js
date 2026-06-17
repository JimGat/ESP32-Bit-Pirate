export class Bpio2SerialTransport {
  constructor({ baudRate = 3_000_000, bufferSize = 131072, maxFrameSize = 131072 } = {}) {
    this.baudRate = baudRate;
    this.bufferSize = bufferSize;
    this.maxFrameSize = maxFrameSize;
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.reading = false;
    this.currentFrame = [];
    this.frames = [];
    this.waiters = [];
    this.overflow = false;
  }

  static isSupported() {
    return "serial" in navigator;
  }

  async requestAndOpen() {
    if (!Bpio2SerialTransport.isSupported()) {
      throw new Error("Web Serial is not available in this browser.");
    }

    this.port = await navigator.serial.requestPort();
    await this.port.open({
      baudRate: this.baudRate,
      dataBits: 8,
      stopBits: 1,
      parity: "none",
      flowControl: "none",
      bufferSize: this.bufferSize,
    });

    this.writer = this.port.writable.getWriter();
    try {
      await this.port.setSignals({ dataTerminalReady: true, requestToSend: false });
    } catch {
      // Some browser/driver combinations do not expose signal control.
    }
    this.startReadLoop();
  }

  async close() {
    this.reading = false;
    const errors = [];

    if (this.reader) {
      try { await this.reader.cancel(); } catch (error) { errors.push(error); }
      try { this.reader.releaseLock(); } catch (error) { errors.push(error); }
      this.reader = null;
    }

    if (this.writer) {
      try { await this.writer.close(); } catch (error) { errors.push(error); }
      try { this.writer.releaseLock(); } catch (error) { errors.push(error); }
      this.writer = null;
    }

    if (this.port) {
      try { await this.port.close(); } catch (error) { errors.push(error); }
      this.port = null;
    }

    this.currentFrame = [];
    this.frames = [];
    this.rejectWaiters(new Error("Serial port closed."));

    if (errors.length) throw new Error(`Serial port closed with warnings: ${errors[0].message}`);
  }

  async write(bytes) {
    if (!this.writer) throw new Error("Serial writer is not available.");
    await this.writer.write(bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes));
  }

  readFrame(timeoutMs = 5000) {
    if (this.frames.length) return Promise.resolve(this.frames.shift());
    if (!this.reading) return Promise.reject(new Error("Serial reader is not available."));

    return new Promise((resolve, reject) => {
      const waiter = {
        resolve,
        reject,
        timeoutId: window.setTimeout(() => {
          this.waiters = this.waiters.filter((item) => item !== waiter);
          reject(new Error("Timeout waiting for BPIO2 response."));
        }, timeoutMs),
      };
      this.waiters.push(waiter);
    });
  }

  startReadLoop() {
    if (this.reading || !this.port?.readable) return;
    this.reading = true;
    void this.readLoop();
  }

  async readLoop() {
    try {
      this.reader = this.port.readable.getReader();
      while (this.reading) {
        const { value, done } = await this.reader.read();
        if (done) break;
        if (value) this.pushBytes(value);
      }
    } catch (error) {
      if (this.reading) this.rejectWaiters(new Error(`Serial read failed: ${error.message}`));
    } finally {
      this.reading = false;
      try { this.reader?.releaseLock(); } catch { /* Already released. */ }
      this.reader = null;
    }
  }

  pushBytes(bytes) {
    for (const byte of bytes) {
      if (byte === 0) {
        if (!this.overflow && this.currentFrame.length) {
          this.pushFrame(Uint8Array.from(this.currentFrame));
        }
        this.currentFrame = [];
        this.overflow = false;
        continue;
      }

      if (this.overflow) continue;
      if (this.currentFrame.length >= this.maxFrameSize) {
        this.currentFrame = [];
        this.overflow = true;
        continue;
      }
      this.currentFrame.push(byte);
    }
  }

  pushFrame(frame) {
    const waiter = this.waiters.shift();
    if (waiter) {
      window.clearTimeout(waiter.timeoutId);
      waiter.resolve(frame);
    } else {
      this.frames.push(frame);
    }
  }

  rejectWaiters(error) {
    for (const waiter of this.waiters.splice(0)) {
      window.clearTimeout(waiter.timeoutId);
      waiter.reject(error);
    }
  }
}
