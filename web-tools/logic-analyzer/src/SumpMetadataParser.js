const STRING_KEYS = new Map([
  [0x01, "deviceName"],
  [0x02, "firmwareVersion"],
  [0x03, "ancillaryVersion"],
]);

const UINT32_KEYS = new Map([
  [0x20, "probeCount"],
  [0x21, "sampleMemoryBytes"],
  [0x22, "dynamicMemoryBytes"],
  [0x23, "maxSampleRateHz"],
  [0x24, "protocolVersion"],
]);

export function parseSumpMetadata(bytes) {
  const metadata = {
    raw: Array.from(bytes),
    unknown: [],
  };

  let offset = 0;
  while (offset < bytes.length) {
    const key = bytes[offset];
    offset += 1;

    if (key === 0x00) {
      metadata.terminatorSeen = true;
      return metadata;
    }

    if (STRING_KEYS.has(key)) {
      const end = findNull(bytes, offset);
      if (end < 0) {
        throw new Error("Invalid SUMP metadata string: missing terminator.");
      }
      metadata[STRING_KEYS.get(key)] = ascii(bytes.subarray(offset, end));
      offset = end + 1;
      continue;
    }

    if (UINT32_KEYS.has(key)) {
      if (offset + 4 > bytes.length) {
        throw new Error("Invalid SUMP metadata integer: truncated value.");
      }
      metadata[UINT32_KEYS.get(key)] = readBe32(bytes, offset);
      offset += 4;
      continue;
    }

    if (key >= 0x40 && key <= 0x5f) {
      if (offset >= bytes.length) {
        throw new Error("Invalid SUMP metadata byte: truncated value.");
      }
      metadata.unknown.push({ key, value: bytes[offset] });
      offset += 1;
      continue;
    }

    if (key >= 0x60 && key <= 0x7f) {
      if (offset + 4 > bytes.length) {
        throw new Error("Invalid SUMP metadata integer: truncated unknown value.");
      }
      metadata.unknown.push({ key, value: readBe32(bytes, offset) });
      offset += 4;
      continue;
    }

    const end = findNull(bytes, offset);
    if (end < 0) {
      metadata.unknown.push({ key, value: Array.from(bytes.subarray(offset)) });
      return metadata;
    }
    metadata.unknown.push({ key, value: ascii(bytes.subarray(offset, end)) });
    offset = end + 1;
  }

  return metadata;
}

function findNull(bytes, offset) {
  for (let index = offset; index < bytes.length; index += 1) {
    if (bytes[index] === 0x00) {
      return index;
    }
  }
  return -1;
}

function readBe32(bytes, offset) {
  return ((bytes[offset] << 24) >>> 0)
    | (bytes[offset + 1] << 16)
    | (bytes[offset + 2] << 8)
    | bytes[offset + 3];
}

function ascii(bytes) {
  return new TextDecoder("ascii").decode(bytes);
}
