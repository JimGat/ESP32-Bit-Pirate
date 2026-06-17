export const BPIO2_VERSION_MAJOR = 2;
export const BPIO2_MINIMUM_VERSION_MINOR = 2;

export const RequestType = Object.freeze({
  STATUS: 1,
  CONFIGURATION: 2,
  DATA: 3,
});

export const ResponseType = Object.freeze({
  STATUS: 1,
  CONFIGURATION: 2,
  DATA: 3,
});

const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();

export class FlatBufferWriter {
  constructor(capacity = 512) {
    this.buffer = new Uint8Array(capacity);
    this.view = new DataView(this.buffer.buffer);
    this.position = 4;
    this.valid = capacity >= 4;
  }

  size() {
    return this.valid ? this.position : 0;
  }

  output() {
    return this.valid ? this.buffer.slice(0, this.position) : new Uint8Array();
  }

  createRootTable(fieldCount) {
    const table = this.createTable(fieldCount);
    if (this.valid) {
      this.writeU32(0, table.object);
    }
    return table;
  }

  createTable(fieldCount) {
    if (!this.valid || fieldCount > 64) {
      return this.failTable();
    }

    this.align(2);
    const vtable = this.position;
    const vtableSize = 4 + fieldCount * 2;
    const objectSize = 4 + fieldCount * 4;
    if (!this.reserve(vtableSize)) {
      return this.failTable();
    }
    this.writeU16(vtable, vtableSize);
    this.writeU16(vtable + 2, objectSize);

    this.align(4);
    const object = this.position;
    if (!this.reserve(objectSize)) {
      return this.failTable();
    }
    this.writeU32(object, object - vtable);

    return { object, fieldCount };
  }

  setU8(table, slot, value, present = true) {
    if (!present) return;
    const field = this.markField(table, slot);
    if (field >= 0) this.buffer[field] = value & 0xff;
  }

  setBool(table, slot, value, present = true) {
    this.setU8(table, slot, value ? 1 : 0, present);
  }

  setU16(table, slot, value, present = true) {
    if (!present) return;
    const field = this.markField(table, slot);
    if (field >= 0) this.writeU16(field, value);
  }

  setU32(table, slot, value, present = true) {
    if (!present) return;
    const field = this.markField(table, slot);
    if (field >= 0) this.writeU32(field, value);
  }

  setOffset(table, slot, target) {
    const field = this.markField(table, slot);
    if (field < 0 || target <= field || target - field > 0xffffffff) {
      this.fail();
      return;
    }
    this.writeU32(field, target - field);
  }

  createString(value = "") {
    const bytes = textEncoder.encode(String(value));
    this.align(4);
    const target = this.position;
    if (!this.reserve(4 + bytes.length + 1)) return 0;
    this.writeU32(target, bytes.length);
    this.buffer.set(bytes, target + 4);
    this.buffer[target + 4 + bytes.length] = 0;
    return target;
  }

  createByteVector(values = []) {
    const bytes = values instanceof Uint8Array ? values : Uint8Array.from(values);
    this.align(4);
    const target = this.position;
    if (!this.reserve(4 + bytes.length)) return 0;
    this.writeU32(target, bytes.length);
    this.buffer.set(bytes, target + 4);
    return target;
  }

  createU32Vector(values = []) {
    const items = [...values];
    this.align(4);
    const target = this.position;
    if (!this.reserve(4 + items.length * 4)) return 0;
    this.writeU32(target, items.length);
    items.forEach((value, index) => this.writeU32(target + 4 + index * 4, value));
    return target;
  }

  createStringVector(values = []) {
    const items = [...values];
    this.align(4);
    const target = this.position;
    if (!this.reserve(4 + items.length * 4)) return 0;
    this.writeU32(target, items.length);
    items.forEach((value, index) => {
      const element = target + 4 + index * 4;
      const stringTarget = this.createString(value);
      if (!this.valid || stringTarget <= element) {
        this.fail();
        return;
      }
      this.writeU32(element, stringTarget - element);
    });
    return target;
  }

  setFloat32(table, slot, value, present = true) {
    if (!present) return;
    const field = this.markField(table, slot);
    if (field >= 0) this.view.setFloat32(field, Number(value), true);
  }

  reserve(count) {
    if (!this.valid || count < 0 || this.position + count > this.buffer.length) {
      this.fail();
      return false;
    }
    this.buffer.fill(0, this.position, this.position + count);
    this.position += count;
    return true;
  }

  align(alignment) {
    if (!this.valid) return;
    const padding = (alignment - (this.position % alignment)) % alignment;
    this.reserve(padding);
  }

  markField(table, slot) {
    if (!this.valid || !table || slot < 0 || slot >= table.fieldCount) {
      this.fail();
      return -1;
    }
    const fieldOffset = 4 + slot * 4;
    const vtable = table.object - this.readU32(table.object);
    this.writeU16(vtable + 4 + slot * 2, fieldOffset);
    return table.object + fieldOffset;
  }

  failTable() {
    this.fail();
    return { object: 0, fieldCount: 0 };
  }

  fail() {
    this.valid = false;
    this.position = 0;
  }

  readU32(offset) {
    return this.view.getUint32(offset, true);
  }

  writeU16(offset, value) {
    this.view.setUint16(offset, value & 0xffff, true);
  }

  writeU32(offset, value) {
    this.view.setUint32(offset, value >>> 0, true);
  }
}

export class TableView {
  constructor(data, objectPosition) {
    this.bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
    this.view = new DataView(this.bytes.buffer, this.bytes.byteOffset, this.bytes.byteLength);
    this.object = objectPosition;

    if (objectPosition < 4 || objectPosition + 4 > this.bytes.length) {
      throw new Error("Invalid FlatBuffers object position.");
    }

    const backOffset = this.view.getInt32(objectPosition, true);
    if (backOffset <= 0 || backOffset > objectPosition) {
      throw new Error("Invalid FlatBuffers vtable offset.");
    }

    this.vtable = objectPosition - backOffset;
    if (this.vtable + 4 > this.bytes.length) {
      throw new Error("FlatBuffers vtable is out of range.");
    }

    this.vtableSize = this.view.getUint16(this.vtable, true);
    this.objectSize = this.view.getUint16(this.vtable + 2, true);
    if (
      this.vtableSize < 4
      || this.vtable + this.vtableSize > this.bytes.length
      || this.objectSize < 4
      || this.object + this.objectSize > this.bytes.length
    ) {
      throw new Error("Invalid FlatBuffers table size.");
    }
  }

  has(slot, width = 1) {
    const entry = this.vtable + 4 + slot * 2;
    if (entry + 2 > this.vtable + this.vtableSize) return false;
    const offset = this.view.getUint16(entry, true);
    return offset !== 0
      && offset + width <= this.objectSize
      && this.object + offset + width <= this.bytes.length;
  }

  fieldPosition(slot, width) {
    if (!this.has(slot, width)) return -1;
    return this.object + this.view.getUint16(this.vtable + 4 + slot * 2, true);
  }

  getU8(slot, fallback = 0) {
    const position = this.fieldPosition(slot, 1);
    return position < 0 ? fallback : this.bytes[position];
  }

  getU16(slot, fallback = 0) {
    const position = this.fieldPosition(slot, 2);
    return position < 0 ? fallback : this.view.getUint16(position, true);
  }

  getU32(slot, fallback = 0) {
    const position = this.fieldPosition(slot, 4);
    return position < 0 ? fallback : this.view.getUint32(position, true);
  }

  getFloat32(slot, fallback = 0) {
    const position = this.fieldPosition(slot, 4);
    return position < 0 ? fallback : this.view.getFloat32(position, true);
  }

  getBool(slot, fallback = false) {
    return this.getU8(slot, fallback ? 1 : 0) !== 0;
  }

  getOffsetTarget(slot) {
    const position = this.fieldPosition(slot, 4);
    if (position < 0) return -1;
    const relative = this.view.getUint32(position, true);
    const target = position + relative;
    if (!relative || target < 0 || target >= this.bytes.length) return -1;
    return target;
  }

  getTable(slot) {
    const target = this.getOffsetTarget(slot);
    return target < 0 ? null : new TableView(this.bytes, target);
  }

  getString(slot) {
    const target = this.getOffsetTarget(slot);
    if (target < 0 || target + 4 > this.bytes.length) return null;
    const length = this.view.getUint32(target, true);
    const start = target + 4;
    const end = start + length;
    if (end >= this.bytes.length) throw new Error("FlatBuffers string is out of range.");
    return textDecoder.decode(this.bytes.subarray(start, end));
  }

  getByteVector(slot) {
    const target = this.getOffsetTarget(slot);
    if (target < 0 || target + 4 > this.bytes.length) return new Uint8Array();
    const length = this.view.getUint32(target, true);
    const start = target + 4;
    const end = start + length;
    if (end > this.bytes.length) throw new Error("FlatBuffers byte vector is out of range.");
    return this.bytes.slice(start, end);
  }

  getU32Vector(slot) {
    const target = this.getOffsetTarget(slot);
    if (target < 0 || target + 4 > this.bytes.length) return [];
    const length = this.view.getUint32(target, true);
    const start = target + 4;
    if (start + length * 4 > this.bytes.length) throw new Error("FlatBuffers uint32 vector is out of range.");
    return Array.from({ length }, (_, index) => this.view.getUint32(start + index * 4, true));
  }

  getStringVector(slot) {
    const target = this.getOffsetTarget(slot);
    if (target < 0 || target + 4 > this.bytes.length) return [];
    const length = this.view.getUint32(target, true);
    const start = target + 4;
    if (start + length * 4 > this.bytes.length) throw new Error("FlatBuffers string vector is out of range.");

    return Array.from({ length }, (_, index) => {
      const element = start + index * 4;
      const stringTarget = element + this.view.getUint32(element, true);
      if (stringTarget + 4 > this.bytes.length) throw new Error("FlatBuffers string vector entry is invalid.");
      const stringLength = this.view.getUint32(stringTarget, true);
      const stringStart = stringTarget + 4;
      const stringEnd = stringStart + stringLength;
      if (stringEnd > this.bytes.length) throw new Error("FlatBuffers string vector entry is out of range.");
      return textDecoder.decode(this.bytes.subarray(stringStart, stringEnd));
    });
  }
}

export function openRootTable(data) {
  const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
  if (bytes.length < 8) throw new Error("FlatBuffers packet is too short.");
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const rootOffset = view.getUint32(0, true);
  if (rootOffset < 4 || rootOffset >= bytes.length) throw new Error("Invalid FlatBuffers root offset.");
  return new TableView(bytes, rootOffset);
}

export function buildStatusRequest() {
  const writer = new FlatBufferWriter(128);
  const root = writer.createRootTable(4);
  writer.setU8(root, 0, BPIO2_VERSION_MAJOR);
  writer.setU16(root, 1, BPIO2_MINIMUM_VERSION_MINOR);
  writer.setU8(root, 2, RequestType.STATUS);

  const status = writer.createTable(1);
  const query = writer.createByteVector([0]);
  writer.setOffset(status, 0, query);
  writer.setOffset(root, 3, status.object);
  return requireWriter(writer);
}

export function buildConfigurationRequest(options = {}) {
  const writer = new FlatBufferWriter(1024);
  const root = writer.createRootTable(4);
  writer.setU8(root, 0, BPIO2_VERSION_MAJOR);
  writer.setU16(root, 1, BPIO2_MINIMUM_VERSION_MINOR);
  writer.setU8(root, 2, RequestType.CONFIGURATION);

  const configuration = writer.createTable(20);
  writer.setOffset(root, 3, configuration.object);

  if (options.mode != null) {
    const mode = writer.createString(options.mode);
    writer.setOffset(configuration, 0, mode);
  }

  if (options.modeConfiguration) {
    const source = options.modeConfiguration;
    const modeConfiguration = writer.createTable(13);
    writer.setU32(modeConfiguration, 0, source.speed ?? 20_000, source.speed != null);
    writer.setU8(modeConfiguration, 1, source.dataBits ?? 8, source.dataBits != null);
    writer.setBool(modeConfiguration, 2, source.parity ?? false, source.parity != null);
    writer.setU8(modeConfiguration, 3, source.stopBits ?? 1, source.stopBits != null);
    writer.setBool(modeConfiguration, 4, source.flowControl ?? false, source.flowControl != null);
    writer.setBool(modeConfiguration, 5, source.signalInversion ?? false, source.signalInversion != null);
    writer.setBool(modeConfiguration, 6, source.clockStretch ?? false, source.clockStretch != null);
    writer.setBool(modeConfiguration, 7, source.clockPolarity ?? false, source.clockPolarity != null);
    writer.setBool(modeConfiguration, 8, source.clockPhase ?? false, source.clockPhase != null);
    writer.setBool(modeConfiguration, 9, source.chipSelectIdle ?? true, source.chipSelectIdle != null);
    writer.setU8(modeConfiguration, 10, source.submode ?? 0, source.submode != null);
    writer.setU32(modeConfiguration, 11, source.txModulation ?? 0, source.txModulation != null);
    writer.setU8(modeConfiguration, 12, source.rxSensor ?? 0, source.rxSensor != null);
    writer.setOffset(configuration, 1, modeConfiguration.object);
  }

  writer.setBool(configuration, 2, true, options.bitOrderMsb === true);
  writer.setBool(configuration, 3, true, options.bitOrderMsb === false);
  writer.setBool(configuration, 4, true, options.psuDisable === true);
  writer.setBool(configuration, 5, true, options.psuEnable === true);
  writer.setU32(configuration, 6, options.psuSetMv ?? 0, options.psuSetMv != null);
  writer.setU16(configuration, 7, options.psuSetMa ?? 300, options.psuSetMa != null);
  writer.setBool(configuration, 8, true, options.pullupDisable === true);
  writer.setBool(configuration, 9, true, options.pullupEnable === true);
  writer.setU8(configuration, 10, options.ioDirectionMask ?? 0, options.ioDirectionMask != null);
  writer.setU8(configuration, 11, options.ioDirection ?? 0, options.ioDirection != null);
  writer.setU8(configuration, 12, options.ioValueMask ?? 0, options.ioValueMask != null);
  writer.setU8(configuration, 13, options.ioValue ?? 0, options.ioValue != null);

  return requireWriter(writer);
}

export function buildDataRequest(options = {}) {
  const payload = options.dataWrite instanceof Uint8Array
    ? options.dataWrite
    : Uint8Array.from(options.dataWrite ?? []);
  const writer = new FlatBufferWriter(Math.max(256, payload.length + 192));
  const root = writer.createRootTable(4);
  writer.setU8(root, 0, BPIO2_VERSION_MAJOR);
  writer.setU16(root, 1, BPIO2_MINIMUM_VERSION_MINOR);
  writer.setU8(root, 2, RequestType.DATA);

  const data = writer.createTable(6);
  writer.setOffset(root, 3, data.object);
  writer.setBool(data, 0, true, options.startMain === true);
  writer.setBool(data, 1, true, options.startAlt === true);
  if (payload.length > 0) {
    const vector = writer.createByteVector(payload);
    writer.setOffset(data, 2, vector);
  }
  writer.setU16(data, 3, options.bytesRead ?? 0, Number(options.bytesRead ?? 0) > 0);
  writer.setBool(data, 4, true, options.stopMain === true);
  writer.setBool(data, 5, true, options.stopAlt === true);
  return requireWriter(writer);
}

export function parseResponsePacket(data, expectedType) {
  const root = openRootTable(data);
  const topError = root.getString(0);
  if (topError) throw new Error(topError);

  const responseType = root.getU8(1, 0);
  if (responseType !== expectedType) {
    throw new Error(`Unexpected BPIO2 response type ${responseType}; expected ${expectedType}.`);
  }

  const contents = root.getTable(2);
  if (!contents) throw new Error("BPIO2 response has no contents.");
  return contents;
}

export function parseStatusResponse(data) {
  const table = parseResponsePacket(data, ResponseType.STATUS);
  const error = table.getString(0);
  if (error) throw new Error(error);

  return {
    versionFlatbuffersMajor: table.getU8(1),
    versionFlatbuffersMinor: table.getU16(2),
    versionHardwareMajor: table.getU8(3),
    versionHardwareMinor: table.getU8(4),
    versionFirmwareMajor: table.getU8(5),
    versionFirmwareMinor: table.getU8(6),
    versionFirmwareGitHash: table.getString(7) ?? "",
    versionFirmwareDate: table.getString(8) ?? "",
    modesAvailable: table.getStringVector(9),
    modeCurrent: table.getString(10) ?? "Unknown",
    modePinLabels: table.getStringVector(11),
    modeBitOrderMsb: table.getBool(12, true),
    modeMaxPacketSize: table.getU32(13),
    modeMaxWrite: table.getU32(14),
    modeMaxRead: table.getU32(15),
    psuEnabled: table.getBool(16),
    psuSetMv: table.getU32(17),
    psuSetMa: table.getU32(18),
    psuMeasuredMv: table.getU32(19),
    psuMeasuredMa: table.getU32(20),
    psuCurrentError: table.getBool(21),
    pullupEnabled: table.getBool(22),
    adcMv: table.getU32Vector(23),
    ioDirection: table.getU8(24),
    ioValue: table.getU8(25),
    diskSizeMb: table.getFloat32(26),
    diskUsedMb: table.getFloat32(27),
    ledCount: table.getU8(28),
  };
}

export function parseConfigurationResponse(data) {
  const table = parseResponsePacket(data, ResponseType.CONFIGURATION);
  return { error: table.getString(0) ?? "" };
}

export function parseDataResponse(data) {
  const table = parseResponsePacket(data, ResponseType.DATA);
  return {
    error: table.getString(0) ?? "",
    data: table.getByteVector(1),
    isAsync: table.getBool(2, false),
  };
}

export function cobsEncode(input) {
  const bytes = input instanceof Uint8Array ? input : Uint8Array.from(input);
  const output = new Uint8Array(bytes.length + Math.ceil(bytes.length / 254) + 1);
  let readIndex = 0;
  let writeIndex = 1;
  let codeIndex = 0;
  let code = 1;

  while (readIndex < bytes.length) {
    if (bytes[readIndex] === 0) {
      output[codeIndex] = code;
      codeIndex = writeIndex;
      writeIndex += 1;
      code = 1;
      readIndex += 1;
    } else {
      output[writeIndex++] = bytes[readIndex++];
      code += 1;
      if (code === 0xff) {
        output[codeIndex] = code;
        codeIndex = writeIndex;
        writeIndex += 1;
        code = 1;
      }
    }
  }

  output[codeIndex] = code;
  return output.slice(0, writeIndex);
}

export function cobsDecode(input) {
  const bytes = input instanceof Uint8Array ? input : Uint8Array.from(input);
  const output = new Uint8Array(bytes.length);
  let readIndex = 0;
  let writeIndex = 0;

  while (readIndex < bytes.length) {
    const code = bytes[readIndex++];
    if (code === 0) throw new Error("Invalid zero byte in COBS frame.");
    const copyLength = code - 1;
    if (readIndex + copyLength > bytes.length) throw new Error("Truncated COBS frame.");
    output.set(bytes.subarray(readIndex, readIndex + copyLength), writeIndex);
    readIndex += copyLength;
    writeIndex += copyLength;
    if (code !== 0xff && readIndex < bytes.length) {
      output[writeIndex++] = 0;
    }
  }

  return output.slice(0, writeIndex);
}

function requireWriter(writer) {
  if (!writer.valid) throw new Error("Could not build BPIO2 FlatBuffers packet.");
  return writer.output();
}
