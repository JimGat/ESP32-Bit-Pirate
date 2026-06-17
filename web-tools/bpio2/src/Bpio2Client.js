import {
  ResponseType,
  buildConfigurationRequest,
  buildDataRequest,
  buildStatusRequest,
  cobsDecode,
  cobsEncode,
  parseConfigurationResponse,
  parseDataResponse,
  parseStatusResponse,
} from "./Bpio2Codec.js";
import { Bpio2SerialTransport } from "./Bpio2SerialTransport.js";

export class Bpio2Client {
  constructor({ log = () => {}, timeoutMs = 6000 } = {}) {
    this.log = log;
    this.timeoutMs = timeoutMs;
    this.transport = new Bpio2SerialTransport();
    this.connected = false;
    this.status = null;
    this.exchangeQueue = Promise.resolve();
  }

  static isSupported() {
    return Bpio2SerialTransport.isSupported();
  }

  async connect() {
    await this.transport.requestAndOpen();
    this.connected = true;
    await delay(120);
    this.status = await this.getStatus();
    return this.status;
  }

  async disconnect({ returnToHiZ = true } = {}) {
    if (returnToHiZ && this.connected) {
      try { await this.configureHiZ(); } catch { /* Best effort. */ }
    }
    this.connected = false;
    this.status = null;
    await this.transport.close();
  }

  async getStatus() {
    const response = await this.exchange(buildStatusRequest(), ResponseType.STATUS);
    this.status = parseStatusResponse(response);
    return this.status;
  }

  async configure(options = {}) {
    const response = await this.exchange(buildConfigurationRequest(options), ResponseType.CONFIGURATION);
    const parsed = parseConfigurationResponse(response);
    if (parsed.error) throw new Error(parsed.error);
    return true;
  }

  async configureHiZ() {
    await this.configure({ mode: "HiZ", modeConfiguration: {} });
    return this.getStatus();
  }

  async configureSpi({ speed = 1_000_000, mode = 0, msbFirst = true, csIdleHigh = true } = {}) {
    await this.configure({
      mode: "SPI",
      bitOrderMsb: msbFirst,
      modeConfiguration: {
        speed,
        dataBits: 8,
        clockPolarity: Boolean(mode & 0x02),
        clockPhase: Boolean(mode & 0x01),
        chipSelectIdle: csIdleHigh,
      },
    });
    return this.getStatus();
  }

  async configureI2c({ speed = 100_000, clockStretch = false } = {}) {
    await this.configure({
      mode: "I2C",
      modeConfiguration: { speed, clockStretch },
    });
    return this.getStatus();
  }

  async configureGpio({ directionMask, direction, valueMask, value } = {}) {
    await this.configure({
      ioDirectionMask: directionMask,
      ioDirection: direction,
      ioValueMask: valueMask,
      ioValue: value,
    });
    return this.getStatus();
  }

  async dataRequest(options = {}, { allowError = false } = {}) {
    const response = await this.exchange(buildDataRequest(options), ResponseType.DATA);
    const parsed = parseDataResponse(response);
    if (parsed.error && !allowError) throw new Error(parsed.error);
    return { ok: !parsed.error, ...parsed };
  }

  async spiTransfer({ tx = new Uint8Array(), readBytes = 0, duplex = false } = {}) {
    return this.dataRequest({
      startMain: !duplex,
      startAlt: duplex,
      dataWrite: tx,
      bytesRead: readBytes,
      stopMain: true,
    });
  }

  async i2cTransfer({ address, write = new Uint8Array(), readBytes = 0 } = {}) {
    const addressByte = (Number(address) & 0x7f) << 1;
    const payload = new Uint8Array(1 + write.length);
    payload[0] = addressByte;
    payload.set(write, 1);
    return this.dataRequest({
      startMain: true,
      dataWrite: payload,
      bytesRead: readBytes,
      stopMain: true,
    });
  }

  async i2cScan({ start = 0x08, end = 0x77, onProgress = () => {} } = {}) {
    const found = [];
    const total = end - start + 1;
    for (let address = start; address <= end; address += 1) {
      const result = await this.i2cProbe(address);
      if (result) found.push(address);
      onProgress({ address, found: [...found], progress: (address - start + 1) / total });
    }
    return found;
  }

  async i2cProbe(address) {
    const addressByte = Uint8Array.of((Number(address) & 0x7f) << 1);
    const result = await this.dataRequest({
      startMain: true,
      dataWrite: addressByte,
      stopMain: true,
    }, { allowError: true });
    return result.ok;
  }

  exchange(request, expectedType) {
    const run = () => this.exchangeNow(request, expectedType);
    const operation = this.exchangeQueue.then(run, run);
    this.exchangeQueue = operation.catch(() => {});
    return operation;
  }

  async exchangeNow(request, expectedType) {
    if (!this.connected) throw new Error("BPIO2 is not connected.");
    const encoded = cobsEncode(request);
    const packet = new Uint8Array(encoded.length + 1);
    packet.set(encoded);
    packet[packet.length - 1] = 0;

    this.log(`TX ${request.length} decoded bytes (${packet.length} framed).`);
    await this.transport.write(packet);
    const frame = await this.transport.readFrame(this.timeoutMs);
    const decoded = cobsDecode(frame);
    this.log(`RX ${decoded.length} decoded bytes.`);

    // Parse once here to validate the expected response type. The specific
    // response parser performs the same check and extracts the payload.
    if (![ResponseType.STATUS, ResponseType.CONFIGURATION, ResponseType.DATA].includes(expectedType)) {
      throw new Error("Invalid expected BPIO2 response type.");
    }
    return decoded;
  }
}

function delay(ms) {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}
