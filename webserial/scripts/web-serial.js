const DEFAULT_SERIAL_OPTIONS = Object.freeze({
  baudRate: 115200,
  dataBits: 8,
  stopBits: 1,
  parity: "none",
  flowControl: "none",
  bufferSize: 8192
});

export class WebSerialConnection extends EventTarget {
  constructor() {
    super();
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.reading = false;
    this.readLoopPromise = null;
    this.writeChain = Promise.resolve();
    this.disconnecting = false;
    this.decoder = null;
    this.handlePhysicalDisconnect = this.handlePhysicalDisconnect.bind(this);

    if (this.isSupported()) {
      navigator.serial.addEventListener("disconnect", this.handlePhysicalDisconnect);
    }
  }

  isSupported() {
    return "serial" in navigator;
  }

  get connected() {
    return Boolean(this.port?.readable && this.port?.writable && !this.disconnecting);
  }

  async connect(options = {}) {
    if (!this.isSupported()) {
      throw new Error("Web Serial is not supported by this browser.");
    }

    if (this.port || this.reading) {
      throw new Error("A serial connection is already active.");
    }

    this.dispatchStatus("requesting", "Select the USB serial port.");
    const selectedPort = await navigator.serial.requestPort();

    try {
      await selectedPort.open({
        ...DEFAULT_SERIAL_OPTIONS,
        ...options
      });

      this.port = selectedPort;
      this.disconnecting = false;
      this.dispatchStatus("connected", this.describePort(selectedPort));
      this.startReadLoop();
      return selectedPort;
    } catch (error) {
      this.port = null;
      throw error;
    }
  }

  async disconnect(reason = "Disconnected") {
    const activePort = this.port;
    this.disconnecting = true;

    if (this.reader) {
      try {
        await this.reader.cancel();
      } catch (error) {
        this.dispatchError(error);
      }
    }

    if (this.readLoopPromise) {
      await this.readLoopPromise.catch((error) => this.dispatchError(error));
    }

    await this.writeChain.catch((error) => this.dispatchError(error));

    if (this.writer) {
      try {
        this.writer.releaseLock();
      } catch {
        // The lock may already be released after a physical disconnect.
      }
      this.writer = null;
    }

    if (activePort) {
      try {
        await activePort.close();
      } catch (error) {
        if (!this.isBenignCloseError(error)) {
          this.dispatchError(error);
        }
      }
    }

    this.port = null;
    this.reader = null;
    this.reading = false;
    this.readLoopPromise = null;
    this.decoder = null;
    this.disconnecting = false;
    this.dispatchStatus("disconnected", reason);
  }

  async write(text) {
    if (!text || !this.port?.writable) {
      return;
    }

    this.writeChain = this.writeChain.catch(() => {}).then(async () => {
      if (!this.port?.writable || this.disconnecting) {
        return;
      }

      this.writer = this.port.writable.getWriter();

      try {
        await this.writer.write(new TextEncoder().encode(text));
      } finally {
        this.writer.releaseLock();
        this.writer = null;
      }
    });

    await this.writeChain;
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
    this.decoder = new TextDecoder("utf-8", { fatal: false });
    this.readLoopPromise = this.readLoop();
  }

  async readLoop() {
    try {
      while (this.port?.readable && !this.disconnecting) {
        this.reader = this.port.readable.getReader();

        try {
          while (!this.disconnecting) {
            const { value, done } = await this.reader.read();

            if (done) {
              break;
            }

            if (value) {
              const text = this.decoder.decode(value, { stream: true });
              if (text) {
                this.dispatchEvent(new CustomEvent("data", { detail: text }));
              }
            }
          }
        } catch (error) {
          if (!this.disconnecting) {
            this.dispatchError(error);
          }
        } finally {
          try {
            this.reader.releaseLock();
          } catch {
            // Reader can already be detached when the USB device disappears.
          }
          this.reader = null;
        }
      }
    } finally {
      const tail = this.decoder?.decode();
      if (tail) {
        this.dispatchEvent(new CustomEvent("data", { detail: tail }));
      }

      this.reading = false;

      if (this.port && !this.disconnecting) {
        await this.disconnect("Serial device disconnected.");
      }
    }
  }

  async handlePhysicalDisconnect(event) {
    if (event.target !== this.port) {
      return;
    }

    this.dispatchStatus("disconnected", "USB serial device disconnected.");
    await this.disconnect("USB serial device disconnected.");
  }

  describePort(port) {
    const info = port.getInfo();
    const details = [];

    if (info.usbVendorId) {
      details.push(`VID ${info.usbVendorId.toString(16).padStart(4, "0").toUpperCase()}`);
    }

    if (info.usbProductId) {
      details.push(`PID ${info.usbProductId.toString(16).padStart(4, "0").toUpperCase()}`);
    }

    return details.length ? `Connected: ${details.join(" / ")}` : "Connected";
  }

  dispatchStatus(state, message) {
    this.dispatchEvent(new CustomEvent("status", {
      detail: { state, message }
    }));
  }

  dispatchError(error) {
    const message = error instanceof Error ? error.message : String(error);
    this.dispatchEvent(new CustomEvent("error", {
      detail: { message, error }
    }));
  }

  isBenignCloseError(error) {
    const message = error instanceof Error ? error.message : String(error);
    return /already closed|not open|device has been lost|disconnected/i.test(message);
  }
}
