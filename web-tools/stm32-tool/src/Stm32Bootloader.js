import {
  Stm32SerialTimeoutError,
  Stm32SerialTransport,
  sleep,
} from "./Stm32SerialTransport.js";
import {
  formatDeviceId,
  lookupDevice,
  normalizeDetectedFlashSize,
} from "./Stm32DeviceDatabase.js";

export const STM32_ACK = 0x79;
export const STM32_NACK = 0x1f;
export const STM32_BUSY = 0x76;

export const STM32_COMMANDS = Object.freeze({
  GET: 0x00,
  GET_VERSION: 0x01,
  GET_ID: 0x02,
  READ_MEMORY: 0x11,
  GO: 0x21,
  WRITE_MEMORY: 0x31,
  ERASE: 0x43,
  EXTENDED_ERASE: 0x44,
  EXTENDED_ERASE_NO_STRETCH: 0x45,
  SPECIAL: 0x50,
  EXTENDED_SPECIAL: 0x51,
  WRITE_PROTECT: 0x63,
  WRITE_UNPROTECT: 0x73,
  READOUT_PROTECT: 0x82,
  READOUT_UNPROTECT: 0x92,
});

const COMMAND_NAMES = new Map(Object.entries(STM32_COMMANDS).map(([name, value]) => [value, name]));

export class Stm32BootloaderError extends Error {
  constructor(message, details = {}) {
    super(message);
    this.name = "Stm32BootloaderError";
    Object.assign(this, details);
  }
}

export class Stm32NackError extends Stm32BootloaderError {
  constructor(context) {
    super(`STM32 bootloader returned NACK during ${context}.`, { context });
    this.name = "Stm32NackError";
  }
}

export class Stm32Bootloader {
  constructor({ transport = new Stm32SerialTransport(), log = () => {} } = {}) {
    this.transport = transport;
    this.log = log;
    this.commands = new Set();
    this.bootloaderVersion = null;
    this.optionBytes = null;
    this.deviceId = null;
    this.device = null;
    this.flashSize = null;
    this.connected = false;
    this.consoleMode = false;
  }

  static isSupported() {
    return Stm32SerialTransport.isSupported();
  }

  async requestPort() {
    return this.transport.requestPort();
  }

  async connect({ baudRate = 115200, autoBoot = null, requestPort = true, signal } = {}) {
    throwIfAborted(signal);
    if (!this.transport.port && requestPort) {
      await this.requestPort();
    }
    if (!this.transport.port) {
      throw new Stm32BootloaderError("Select a serial port first.");
    }

    await this.transport.open({
      baudRate,
      dataBits: 8,
      stopBits: 1,
      parity: "even",
      flowControl: "none",
    });
    this.consoleMode = false;
    this.transport.setStreamHandler(null);

    if (autoBoot?.enabled) {
      this.log("Applying BOOT0/NRST sequence through RTS/DTR.");
      await this.enterBootloader(autoBoot, signal);
    }

    await this.transport.flushInput(40);
    await this.synchronize({ signal });
    const info = await this.identify({ signal });
    this.connected = true;
    return info;
  }

  async reconnectBootloader({ baudRate = 115200, autoBoot = null, signal } = {}) {
    if (!this.transport.port) {
      throw new Stm32BootloaderError("No previously selected serial port is available.");
    }
    await this.transport.reopen({
      baudRate,
      dataBits: 8,
      stopBits: 1,
      parity: "even",
      flowControl: "none",
    });
    this.consoleMode = false;
    this.transport.setStreamHandler(null);
    if (autoBoot?.enabled) {
      await this.enterBootloader(autoBoot, signal);
    }
    await this.transport.flushInput(40);
    await this.synchronize({ signal });
    const info = await this.identify({ signal });
    this.connected = true;
    return info;
  }

  async disconnect({ forgetPort = false } = {}) {
    this.connected = false;
    this.consoleMode = false;
    this.commands.clear();
    await this.transport.close({ keepPort: !forgetPort });
  }

  async enterBootloader(config, signal) {
    const bootSignal = config.bootSignal ?? "dataTerminalReady";
    const resetSignal = config.resetSignal ?? "requestToSend";
    if (bootSignal === resetSignal) {
      throw new Stm32BootloaderError("BOOT0 and NRST must use different control signals.");
    }

    const bootActiveLevel = Boolean(config.bootActiveLevel);
    const resetActiveLevel = Boolean(config.resetActiveLevel);
    const makeSignals = (bootActive, resetActive) => ({
      dataTerminalReady: signalValue("dataTerminalReady", bootSignal, resetSignal, bootActive, resetActive, bootActiveLevel, resetActiveLevel),
      requestToSend: signalValue("requestToSend", bootSignal, resetSignal, bootActive, resetActive, bootActiveLevel, resetActiveLevel),
    });

    throwIfAborted(signal);
    await this.transport.setSignals(makeSignals(false, false));
    await sleep(30);
    await this.transport.setSignals(makeSignals(true, false));
    await sleep(40);
    await this.transport.setSignals(makeSignals(true, true));
    await sleep(config.resetPulseMs ?? 100);
    await this.transport.setSignals(makeSignals(true, false));
    await sleep(config.bootHoldMs ?? 160);
    await this.transport.setSignals(makeSignals(false, false));
    await sleep(60);
  }

  async synchronize({ attempts = 3, signal } = {}) {
    let lastError = null;
    for (let attempt = 1; attempt <= attempts; attempt += 1) {
      throwIfAborted(signal);
      await this.transport.flushInput(20);
      this.log(`Synchronizing with STM32 bootloader (${attempt}/${attempts})...`);
      await this.transport.write(Uint8Array.of(0x7f));
      try {
        const response = await this.transport.readByte(1200);
        if (response === STM32_ACK) {
          this.log("Bootloader synchronization acknowledged.");
          return;
        }
        if (response === STM32_NACK) {
          this.log("Bootloader returned NACK to sync; testing command mode.");
          try {
            await this.getSupportedCommands({ signal });
            return;
          } catch (error) {
            lastError = error;
          }
        } else {
          lastError = new Stm32BootloaderError(`Unexpected sync response 0x${hexByte(response)}.`);
        }
      } catch (error) {
        lastError = error;
      }
      await sleep(100);
    }

    const suffix = lastError instanceof Stm32SerialTimeoutError
      ? "No response was received. Check BOOT0, RESET, TX/RX crossover, GND and 8E1 support."
      : lastError?.message ?? "The target did not enter the system bootloader.";
    throw new Stm32BootloaderError(`Unable to synchronize with the STM32 bootloader. ${suffix}`, { cause: lastError });
  }

  async identify({ signal } = {}) {
    throwIfAborted(signal);
    const getInfo = await this.getSupportedCommands({ signal });
    const versionInfo = this.supports(STM32_COMMANDS.GET_VERSION)
      ? await this.getVersionAndProtection({ signal })
      : null;
    const idInfo = await this.getId({ signal });
    const device = lookupDevice(idInfo.deviceId);
    let flashSize = device.defaultFlashSize;

    if (device.flashSizeAddress && this.supports(STM32_COMMANDS.READ_MEMORY)) {
      try {
        const bytes = await this.readMemory(device.flashSizeAddress, 2, { signal, progress: null });
        const flashKilobytes = bytes[0] | (bytes[1] << 8);
        flashSize = normalizeDetectedFlashSize(flashKilobytes, device);
        this.log(`Flash-size register: ${flashKilobytes} KB.`);
      } catch (error) {
        this.log(`Flash-size register unavailable; using ${formatBytes(flashSize)} database default.`);
      }
    }

    this.deviceId = idInfo.deviceId;
    this.device = device;
    this.flashSize = flashSize;
    this.bootloaderVersion = versionInfo?.version ?? getInfo.version;
    this.optionBytes = versionInfo?.optionBytes ?? null;

    return this.getDeviceInfo();
  }

  async refreshInfo({ signal } = {}) {
    return this.identify({ signal });
  }

  getDeviceInfo() {
    const portInfo = this.transport.getInfo();
    return {
      connected: this.connected || Boolean(this.device),
      deviceId: this.deviceId,
      deviceIdText: this.deviceId == null ? "-" : formatDeviceId(this.deviceId),
      name: this.device?.name ?? "Unknown STM32",
      family: this.device?.family ?? "STM32",
      bootloaderVersion: this.bootloaderVersion,
      bootloaderVersionText: formatBootloaderVersion(this.bootloaderVersion),
      optionBytes: this.optionBytes ? [...this.optionBytes] : null,
      optionBytesText: this.optionBytes ? [...this.optionBytes].map((value) => `0x${hexByte(value)}`).join(" / ") : "-",
      commands: [...this.commands].sort((a, b) => a - b),
      commandNames: [...this.commands].sort((a, b) => a - b).map(commandName),
      flashStart: this.device?.flashStart ?? 0x08000000,
      flashSize: this.flashSize ?? this.device?.defaultFlashSize ?? 128 * 1024,
      flashSizeDetected: Boolean(this.device?.flashSizeAddress),
      knownDevice: Boolean(this.device?.known),
      geometry: this.device?.geometry ?? null,
      portInfo,
    };
  }

  async getSupportedCommands({ signal } = {}) {
    throwIfAborted(signal);
    await this.sendCommand(STM32_COMMANDS.GET, "GET", 2000);
    const countMinusOne = await this.transport.readByte(2000);
    const payload = await this.transport.readExact(countMinusOne + 1, 2000);
    await this.expectAck("GET response", 2000);
    if (!payload.length) {
      throw new Stm32BootloaderError("STM32 GET response is empty.");
    }

    const version = payload[0];
    const commands = payload.slice(1);
    this.commands = new Set(commands);
    this.bootloaderVersion = version;
    this.log(`Bootloader ${formatBootloaderVersion(version)} supports ${commands.length} commands.`);
    return { version, commands };
  }


  async getVersionAndProtection({ signal } = {}) {
    throwIfAborted(signal);
    await this.sendCommand(STM32_COMMANDS.GET_VERSION, "GET VERSION", 2000);
    const payload = await this.transport.readExact(3, 2000);
    await this.expectAck("GET VERSION response", 2000);
    const version = payload[0];
    const optionBytes = payload.slice(1);
    this.log(`Protection bytes: ${[...optionBytes].map((value) => `0x${hexByte(value)}`).join(" / ")}.`);
    return { version, optionBytes };
  }

  async getId({ signal } = {}) {
    throwIfAborted(signal);
    await this.sendCommand(STM32_COMMANDS.GET_ID, "GET ID", 2000);
    const countMinusOne = await this.transport.readByte(2000);
    const bytes = await this.transport.readExact(countMinusOne + 1, 2000);
    await this.expectAck("GET ID response", 2000);
    const deviceId = bytes.reduce((value, byte) => ((value << 8) | byte) >>> 0, 0) & 0xffff;
    this.log(`Device ID ${formatDeviceId(deviceId)}.`);
    return { deviceId, bytes };
  }

  async readMemory(address, length, { progress = null, signal } = {}) {
    this.requireCommand(STM32_COMMANDS.READ_MEMORY, "Read Memory");
    validateAddressAndLength(address, length);
    const output = new Uint8Array(length);
    let done = 0;

    while (done < length) {
      throwIfAborted(signal);
      const chunkLength = Math.min(256, length - done);
      await this.sendCommand(STM32_COMMANDS.READ_MEMORY, "Read Memory", 2500);
      await this.sendAddress((address + done) >>> 0, "Read Memory address");
      const encodedLength = chunkLength - 1;
      await this.transport.write(Uint8Array.of(encodedLength, encodedLength ^ 0xff));
      await this.expectAck("Read Memory length", 2500);
      const chunk = await this.transport.readExact(chunkLength, 5000);
      output.set(chunk, done);
      done += chunkLength;
      progress?.({ done, total: length, address: address + done });
    }

    return output;
  }

  async writeMemory(address, bytes, { progress = null, signal } = {}) {
    this.requireCommand(STM32_COMMANDS.WRITE_MEMORY, "Write Memory");
    const input = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    validateAddressAndLength(address, input.length);
    if (address % 4 !== 0) {
      throw new Stm32BootloaderError("STM32 write addresses must be aligned to 4 bytes.");
    }

    let done = 0;
    while (done < input.length) {
      throwIfAborted(signal);
      const sourceLength = Math.min(256, input.length - done);
      const paddedLength = alignUp(sourceLength, 4);
      const chunk = new Uint8Array(paddedLength);
      chunk.fill(0xff);
      chunk.set(input.subarray(done, done + sourceLength));

      await this.sendCommand(STM32_COMMANDS.WRITE_MEMORY, "Write Memory", 2500);
      await this.sendAddress((address + done) >>> 0, "Write Memory address");

      const payload = new Uint8Array(chunk.length + 2);
      payload[0] = chunk.length - 1;
      payload.set(chunk, 1);
      payload[payload.length - 1] = xorChecksum(payload.subarray(0, payload.length - 1));
      await this.transport.write(payload);
      await this.expectAck("Write Memory data", 8000);

      done += sourceLength;
      progress?.({ done, total: input.length, address: address + done });
    }
  }

  async verifyMemory(address, expected, { progress = null, signal } = {}) {
    const bytes = expected instanceof Uint8Array ? expected : new Uint8Array(expected);
    const actual = await this.readMemory(address, bytes.length, { progress, signal });
    for (let index = 0; index < bytes.length; index += 1) {
      if (bytes[index] !== actual[index]) {
        throw new Stm32BootloaderError(
          `Verification failed at 0x${((address + index) >>> 0).toString(16).padStart(8, "0").toUpperCase()}: expected 0x${hexByte(bytes[index])}, read 0x${hexByte(actual[index])}.`,
          { address: address + index, expected: bytes[index], actual: actual[index] },
        );
      }
    }
    return true;
  }

  async massErase({ signal } = {}) {
    throwIfAborted(signal);
    const command = this.selectEraseCommand();
    if (isExtendedEraseCommand(command)) {
      await this.sendCommand(command, "Extended Erase", 2500);
      await this.transport.write(Uint8Array.of(0xff, 0xff, 0x00));
      await this.expectAck("mass erase", 120000);
    } else {
      await this.sendCommand(command, "Erase", 2500);
      await this.transport.write(Uint8Array.of(0xff, 0x00));
      await this.expectAck("mass erase", 120000);
    }
    this.log("Mass erase completed.");
  }

  async erasePages(pageIndexes, { progress = null, signal } = {}) {
    const pages = [...new Set(pageIndexes.map(Number))].sort((a, b) => a - b);
    if (!pages.length) {
      throw new Stm32BootloaderError("No flash pages or sectors were selected for erase.");
    }
    if (pages.some((page) => !Number.isInteger(page) || page < 0 || page > 0xffff)) {
      throw new Stm32BootloaderError("Erase page indexes are invalid.");
    }

    const command = this.selectEraseCommand(pages);
    const batchSize = 256;
    let done = 0;

    for (let offset = 0; offset < pages.length; offset += batchSize) {
      throwIfAborted(signal);
      const batch = pages.slice(offset, offset + batchSize);
      await this.sendCommand(command, isExtendedEraseCommand(command) ? "Extended Erase" : "Erase", 2500);

      if (isExtendedEraseCommand(command)) {
        const payload = new Uint8Array(2 + batch.length * 2 + 1);
        const count = batch.length - 1;
        payload[0] = (count >>> 8) & 0xff;
        payload[1] = count & 0xff;
        batch.forEach((page, index) => {
          payload[2 + index * 2] = (page >>> 8) & 0xff;
          payload[3 + index * 2] = page & 0xff;
        });
        payload[payload.length - 1] = xorChecksum(payload.subarray(0, payload.length - 1));
        await this.transport.write(payload);
      } else {
        if (batch.some((page) => page > 0xff)) {
          throw new Stm32BootloaderError("This bootloader only supports 8-bit page indexes.");
        }
        const payload = new Uint8Array(1 + batch.length + 1);
        payload[0] = batch.length - 1;
        payload.set(batch, 1);
        payload[payload.length - 1] = xorChecksum(payload.subarray(0, payload.length - 1));
        await this.transport.write(payload);
      }

      await this.expectAck("page erase", 60000);
      done += batch.length;
      progress?.({ done, total: pages.length });
    }
    this.log(`Erased ${pages.length} page${pages.length === 1 ? "" : "s"}/sector${pages.length === 1 ? "" : "s"}.`);
  }

  async go(address, { signal } = {}) {
    throwIfAborted(signal);
    this.requireCommand(STM32_COMMANDS.GO, "Go");
    await this.sendCommand(STM32_COMMANDS.GO, "Go", 2500);
    await this.sendAddress(address >>> 0, "Go address");
    this.log(`Execution started at 0x${(address >>> 0).toString(16).padStart(8, "0").toUpperCase()}.`);
  }

  async openConsole({ baudRate = 115200, onData } = {}) {
    if (!this.transport.port) {
      throw new Stm32BootloaderError("Connect to a serial port before opening the console.");
    }
    this.connected = false;
    await this.transport.reopen({
      baudRate,
      dataBits: 8,
      stopBits: 1,
      parity: "none",
      flowControl: "none",
    });
    this.transport.setStreamHandler(onData);
    this.consoleMode = true;
    this.log(`Serial console opened at ${baudRate} baud, 8N1.`);
  }

  async writeConsole(bytes) {
    if (!this.consoleMode) {
      throw new Stm32BootloaderError("The serial console is not open.");
    }
    await this.transport.write(bytes);
  }

  supports(command) {
    return this.commands.has(command);
  }

  requireCommand(command, label) {
    if (this.commands.size && !this.supports(command)) {
      throw new Stm32BootloaderError(`${label} is not supported by this STM32 bootloader.`);
    }
  }

  selectEraseCommand(pages = []) {
    if (this.supports(STM32_COMMANDS.EXTENDED_ERASE)) {
      return STM32_COMMANDS.EXTENDED_ERASE;
    }
    if (this.supports(STM32_COMMANDS.EXTENDED_ERASE_NO_STRETCH)) {
      return STM32_COMMANDS.EXTENDED_ERASE_NO_STRETCH;
    }
    if (this.supports(STM32_COMMANDS.ERASE)) {
      if (pages.some((page) => page > 0xff)) {
        throw new Stm32BootloaderError("Extended Erase is required for page indexes above 255.");
      }
      return STM32_COMMANDS.ERASE;
    }
    throw new Stm32BootloaderError("This STM32 bootloader does not advertise an erase command.");
  }

  async sendCommand(command, context = commandName(command), timeoutMs = 2500) {
    await this.transport.write(encodeCommand(command));
    await this.expectAck(context, timeoutMs);
  }

  async sendAddress(address, context) {
    const packet = encodeAddress(address);
    await this.transport.write(packet);
    await this.expectAck(context, 2500);
  }

  async expectAck(context, timeoutMs) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const remaining = Math.max(1, deadline - Date.now());
      const response = await this.transport.readByte(remaining);
      if (response === STM32_ACK) return;
      if (response === STM32_NACK) throw new Stm32NackError(context);
      if (response === STM32_BUSY) continue;
      throw new Stm32BootloaderError(`Unexpected response 0x${hexByte(response)} during ${context}.`);
    }
    throw new Stm32SerialTimeoutError(`Timed out during ${context}.`);
  }
}

export function encodeCommand(command) {
  const value = Number(command) & 0xff;
  return Uint8Array.of(value, value ^ 0xff);
}

export function encodeAddress(address) {
  const value = Number(address) >>> 0;
  const packet = new Uint8Array(5);
  packet[0] = (value >>> 24) & 0xff;
  packet[1] = (value >>> 16) & 0xff;
  packet[2] = (value >>> 8) & 0xff;
  packet[3] = value & 0xff;
  packet[4] = xorChecksum(packet.subarray(0, 4));
  return packet;
}

export function xorChecksum(bytes) {
  let checksum = 0;
  for (const byte of bytes) checksum ^= byte;
  return checksum & 0xff;
}

export function commandName(command) {
  return COMMAND_NAMES.get(command) ?? `0x${hexByte(command)}`;
}

export function formatBootloaderVersion(version) {
  if (!Number.isFinite(version)) return "-";
  return `v${(version >>> 4) & 0x0f}.${version & 0x0f}`;
}


function isExtendedEraseCommand(command) {
  return command === STM32_COMMANDS.EXTENDED_ERASE || command === STM32_COMMANDS.EXTENDED_ERASE_NO_STRETCH;
}

function signalValue(property, bootSignal, resetSignal, bootActive, resetActive, bootActiveLevel, resetActiveLevel) {
  if (property === bootSignal) return bootActive ? bootActiveLevel : !bootActiveLevel;
  if (property === resetSignal) return resetActive ? resetActiveLevel : !resetActiveLevel;
  return false;
}

function validateAddressAndLength(address, length) {
  if (!Number.isInteger(address) || address < 0 || address > 0xffffffff) {
    throw new Stm32BootloaderError("Memory address is invalid.");
  }
  if (!Number.isInteger(length) || length <= 0) {
    throw new Stm32BootloaderError("Memory length is invalid.");
  }
  if (address + length - 1 > 0xffffffff) {
    throw new Stm32BootloaderError("Memory operation exceeds the 32-bit address space.");
  }
}

function alignUp(value, alignment) {
  return Math.ceil(value / alignment) * alignment;
}

function throwIfAborted(signal) {
  if (signal?.aborted) {
    throw new DOMException("Operation cancelled.", "AbortError");
  }
}

function hexByte(value) {
  return (Number(value) & 0xff).toString(16).padStart(2, "0").toUpperCase();
}

function formatBytes(value) {
  if (value >= 1024 * 1024) return `${(value / (1024 * 1024)).toFixed(1)} MB`;
  if (value >= 1024) return `${Math.round(value / 1024)} KB`;
  return `${value} B`;
}
