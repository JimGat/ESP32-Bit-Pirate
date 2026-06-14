'use strict';

// src/index.ts
var DriverFTDI = class {
  constructor(device) {
    this._internal = {
      emitter: new EventTarget(),
      device,
      endpoints: {
        in: null,
        out: null
      },
      options: {
        baudRate: 9600,
        stopBits: 0,
        parity: 0,
        dataBits: 8
      }
    };
    this.readable = new ReadableStream({
      start: (controller) => {
        this._internal.controller = controller;
      }
    });
    this.writable = new WritableStream({
      write: async (chunk) => {
        await this._send(chunk);
      }
    });
  }
  async open(options) {
    this._internal.options = Object.assign(this._internal.options, options);
    this.readable = new ReadableStream({
      start: (controller) => {
        this._internal.controller = controller;
      }
    });
    this.writable = new WritableStream({
      write: async (chunk) => {
        await this._send(chunk);
      }
    });
    await this._internal.device.open();
    let iface = this._internal.device.configuration?.interfaces[0];
    if (!iface)
      throw new Error("Failed to open device");
    await this._internal.device.claimInterface(iface.interfaceNumber);
    iface.alternate.endpoints.forEach((endpoint) => {
      if (endpoint.direction == "in" && endpoint.type == "bulk") {
        this._internal.endpoints.in = endpoint;
      }
      if (endpoint.direction == "out" && endpoint.type == "bulk") {
        this._internal.endpoints.out = endpoint;
      }
    });
    await this._internal.device.controlTransferOut({
      requestType: "vendor",
      recipient: "device",
      request: 0,
      // SIO_RESET
      value: 0,
      // SIO_RESET_SIO
      index: iface.interfaceNumber
    }, new Uint8Array([]));
    await this._internal.device.controlTransferOut({
      requestType: "vendor",
      recipient: "device",
      request: 11,
      // SIO_SET_BITMODE
      value: 0,
      // BITMODE_RESET
      index: iface.interfaceNumber
    }, new Uint8Array([]));
    let [value, index] = convertBaudrate(this._internal.options.baudRate, this._internal.device, iface.interfaceNumber);
    await this._internal.device.controlTransferOut({
      requestType: "vendor",
      recipient: "device",
      request: 3,
      // SIO_SET_BAUDRATE
      value,
      index
    }, new Uint8Array([]));
    let config = this._internal.options.dataBits & 15;
    config |= this._internal.options.parity << 8;
    config |= this._internal.options.stopBits << 11;
    await this._internal.device.controlTransferOut({
      requestType: "vendor",
      recipient: "device",
      request: 4,
      // SIO_SET_DATA
      value: config,
      index: iface.interfaceNumber
    }, new Uint8Array([]));
    this._poll().then(() => {
      this._internal.emitter.dispatchEvent(new Event("stopped"));
    });
    return this;
  }
  async setSignals(signals) {
    let iface = this._internal.device.configuration?.interfaces[0];
    if (!iface)
      throw new Error("Failed to open device");
    let value = 0;
    if (typeof signals.dataTerminalReady !== "undefined") {
      if (signals.dataTerminalReady)
        value |= 257;
      else
        value |= 256;
    }
    if (typeof signals.requestToSend !== "undefined") {
      if (signals.requestToSend)
        value |= 514;
      else
        value |= 512;
    }
    await this._internal.device.controlTransferOut({
      requestType: "vendor",
      recipient: "device",
      request: 1,
      // FTDIO_SIO_MODEM_CTRL
      value,
      index: iface.interfaceNumber
    });
  }
  close() {
    return new Promise((resolve) => {
      this._internal.emitter.addEventListener("stopped", async () => {
        let iface = this._internal.device.configuration?.interfaces[0];
        if (iface)
          await this._internal.device.releaseInterface(iface.interfaceNumber);
        await this._internal.device.close();
        this.writable = void 0;
        this.readable = void 0;
        resolve();
      }, { once: true });
      this._internal.emitter.dispatchEvent(new Event("closing"));
    });
  }
  getInfo() {
    return {
      usbVendorId: this._internal.device.vendorId,
      usbProductId: this._internal.device.productId
    };
  }
  _send(data) {
    if (!this._internal.endpoints.out)
      throw new Error("Port must be open first!");
    return this._internal.device.transferOut(this._internal.endpoints.out.endpointNumber, data);
  }
  async _poll() {
    if (!this._internal.endpoints.in)
      throw new Error("Port must be open first!");
    let closing = false;
    this._internal.emitter.addEventListener("closing", () => {
      closing = true;
    }, { once: true });
    while (!closing) {
      const transfer = await this._internal.device.transferIn(this._internal.endpoints.in.endpointNumber, 64);
      if (transfer.status === "ok" && transfer.data) {
        if (transfer.data.byteLength > 2) {
          try {
            this._internal.controller?.enqueue(new Uint8Array(transfer.data.buffer).slice(2));
          } catch {
          }
        }
      }
    }
  }
};
function isLegacy(device) {
  return device.deviceVersionMajor < 2;
}
function isModern(device) {
  return [7, 8, 9].includes(device.deviceVersionMajor);
}
function hasMPSSE(device) {
  return [5, 7, 8, 9].includes(device.deviceVersionMajor);
}
function convertBaudrate(baudrate, device, iface) {
  let BAUDRATE_REF_BASE = 3e6;
  let BAUDRATE_REF_HIGH = 12e6;
  let refclock, hispeed;
  if (baudrate < Math.floor(2 * BAUDRATE_REF_BASE / (2 * 16384 + 1)))
    throw new Error("Baudrate too low");
  if (baudrate > BAUDRATE_REF_BASE) {
    if (!isModern(device) || baudrate > BAUDRATE_REF_HIGH)
      throw new Error("Baudrate too high");
    refclock = BAUDRATE_REF_HIGH;
    hispeed = true;
  } else {
    refclock = BAUDRATE_REF_BASE;
    hispeed = false;
  }
  let am_adjust_up = [0, 0, 0, 1, 0, 3, 2, 1];
  let am_adjust_dn = [0, 0, 0, 1, 0, 1, 2, 3];
  let frac_code = [0, 3, 2, 4, 1, 5, 6, 7];
  let divisor = Math.floor(refclock * 8 / baudrate);
  if (isLegacy(device)) {
    divisor -= am_adjust_dn[divisor & 7];
  }
  let best_divisor = 0;
  let best_baud_diff = 0;
  for (let i of [0, 1]) {
    let try_divisor = divisor + i;
    if (!hispeed) {
      if (try_divisor <= 8) {
        try_divisor = 8;
      } else if (isLegacy(device) && try_divisor < 12) {
        try_divisor = 12;
      } else if (try_divisor < 16) {
        try_divisor = 16;
      } else {
        if (isLegacy(device)) {
          try_divisor += am_adjust_up[try_divisor & 7];
          if (try_divisor > 131064) {
            try_divisor = 131064;
          }
        } else {
          if (try_divisor > 131071) {
            try_divisor = 131071;
          }
        }
      }
    }
    let baud_estimate = Math.floor((refclock * 8 + Math.floor(try_divisor / 2)) / try_divisor);
    let baud_diff;
    if (baud_estimate < baudrate)
      baud_diff = baudrate - baud_estimate;
    else
      baud_diff = baud_estimate - baudrate;
    if (i == 0 || baud_diff < best_baud_diff) {
      best_divisor = try_divisor;
      best_baud_diff = baud_diff;
      if (baud_diff == 0) {
        break;
      }
    }
  }
  let encoded_divisor = best_divisor >> 3 | frac_code[best_divisor & 7] << 14;
  if (encoded_divisor == 1)
    encoded_divisor = 0;
  else if (encoded_divisor == 16385)
    encoded_divisor = 1;
  let value = encoded_divisor & 65535;
  let index;
  if (hasMPSSE(device)) {
    index = encoded_divisor >> 8 & 65535;
    index &= 65280;
    index |= iface;
  } else {
    index = encoded_divisor >> 16 & 65535;
  }
  if (hispeed) {
    index |= 1 << 9;
  }
  return [value, index];
}
var src_default = DriverFTDI;

let port;
let opts;
let writer;
let reader;

let onData;
let writeBuffer;
let readBuffer;
let writeAddressBuf;
let readAddressBuf;

const readPromise = (customReader) => new Promise(async () => {
    while (true) {
        const { value, done } = await customReader.read();
        if (done) break

        let address = readAddressBuf[0];
        let read = 0;

        while (read !== value.length) {
            const initialLength = Math.min(value.length, readBuffer.length - address);
            readBuffer.set(value.slice(read, read + initialLength), address);
            read += initialLength;
            address += initialLength;

            if (address === readBuffer.length) {
                address = 3;
            }
        }
        readAddressBuf[0] = address;

        onData && onData();
    }
});

function readFromBuffer(currentAddress, targetAddress, buffer) {
    if (currentAddress < targetAddress) {
        return buffer.slice(currentAddress, targetAddress)
    }

    const array = new Uint8Array(buffer.length - currentAddress + targetAddress - 3);
    array.set(buffer.slice(currentAddress));
    array.set(buffer.slice(3, targetAddress), buffer.length - currentAddress);

    return array
}

addEventListener('message', async msg => {
    try {
        const data = msg.data;

        switch (data.type) {
            case 'clear-read-buffer': {
                const timeoutPromise = new Promise(resolve => setTimeout(resolve, data.timeout));
                const onDone = new Promise(resolve => onData = resolve);

                await Promise.race([timeoutPromise, onDone]);
                readAddressBuf[0] = 3;

                postMessage({type: 'clear-read-buffer'});
                break
            }
            case 'init': {
                if (navigator.serial) {
                    port = (await navigator.serial.getPorts())[data.port];
                } else {
                    const device = (await navigator.usb.getDevices())[data.port];
                    port = new src_default(device);
                }

                readBuffer = new Uint8Array(data.readBuffer);
                writeBuffer = new Uint8Array(data.writeBuffer);
                readAddressBuf = new Uint16Array(data.readBuffer);
                writeAddressBuf = new Uint16Array(data.writeBuffer);

                await port.open(data.options);
                opts = data.options;
                writer = port.writable.getWriter();
                reader = port.readable.getReader();

                let address = 3;
                setInterval(async () => {
                    if (writeAddressBuf[0] === address) return

                    const target = writeAddressBuf[0];
                    const data = readFromBuffer(address, target, writeBuffer);
                    await writer.write(data);

                    address = target;
                }, 0);

                readPromise(reader).then();
                postMessage({type: 'ready'});
                break
            }
            case 'close': {
                writer.releaseLock();
                reader.cancel();
                reader.releaseLock();
                await port.close();
                postMessage({type: 'closed'});
                break
            }
            default: {
                console.error('Unknown message type', data.type);
                break
            }
        }
    } catch (e) {
        console.error(e);
        postMessage({type: 'error', error: e});
    }

});
