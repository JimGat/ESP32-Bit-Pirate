export class IntelHexError extends Error {
  constructor(message, lineNumber = null) {
    super(lineNumber ? `${message} (line ${lineNumber})` : message);
    this.name = "IntelHexError";
    this.lineNumber = lineNumber;
  }
}

export function parseIntelHex(text) {
  const lines = String(text).replace(/\r/g, "").split("\n");
  const records = [];
  let upperAddress = 0;
  let startAddress = null;
  let eofSeen = false;

  for (let index = 0; index < lines.length; index += 1) {
    const lineNumber = index + 1;
    const line = lines[index].trim();
    if (!line) continue;
    if (!line.startsWith(":")) {
      throw new IntelHexError("Intel HEX records must start with ':'", lineNumber);
    }

    const bytes = decodeHexLine(line.slice(1), lineNumber);
    if (bytes.length < 5) {
      throw new IntelHexError("Intel HEX record is too short", lineNumber);
    }

    const length = bytes[0];
    if (bytes.length !== length + 5) {
      throw new IntelHexError("Intel HEX byte count does not match the record length", lineNumber);
    }

    const checksum = bytes.reduce((sum, value) => (sum + value) & 0xff, 0);
    if (checksum !== 0) {
      throw new IntelHexError("Intel HEX checksum is invalid", lineNumber);
    }

    const offset = (bytes[1] << 8) | bytes[2];
    const type = bytes[3];
    const data = bytes.slice(4, 4 + length);

    if (type === 0x00) {
      records.push({ address: upperAddress + offset, bytes: new Uint8Array(data) });
    } else if (type === 0x01) {
      eofSeen = true;
      break;
    } else if (type === 0x02) {
      if (data.length !== 2) throw new IntelHexError("Invalid extended segment address record", lineNumber);
      upperAddress = (((data[0] << 8) | data[1]) << 4) >>> 0;
    } else if (type === 0x04) {
      if (data.length !== 2) throw new IntelHexError("Invalid extended linear address record", lineNumber);
      upperAddress = (((data[0] << 8) | data[1]) << 16) >>> 0;
    } else if (type === 0x03) {
      if (data.length !== 4) throw new IntelHexError("Invalid start segment address record", lineNumber);
      const cs = (data[0] << 8) | data[1];
      const ip = (data[2] << 8) | data[3];
      startAddress = ((cs << 4) + ip) >>> 0;
    } else if (type === 0x05) {
      if (data.length !== 4) throw new IntelHexError("Invalid start linear address record", lineNumber);
      startAddress = ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]) >>> 0;
    }
  }

  if (!records.length) {
    throw new IntelHexError("The Intel HEX file contains no data records.");
  }

  records.sort((a, b) => a.address - b.address);
  const segments = mergeRecords(records);
  return {
    segments,
    startAddress,
    eofSeen,
    totalBytes: segments.reduce((sum, segment) => sum + segment.bytes.length, 0),
  };
}

export function looksLikeIntelHex(fileName, text) {
  return /\.hex$/i.test(fileName ?? "") || String(text).trimStart().startsWith(":");
}

function decodeHexLine(hex, lineNumber) {
  if (hex.length % 2 !== 0 || !/^[0-9a-f]+$/i.test(hex)) {
    throw new IntelHexError("Intel HEX record contains invalid hexadecimal data", lineNumber);
  }
  const bytes = [];
  for (let index = 0; index < hex.length; index += 2) {
    bytes.push(Number.parseInt(hex.slice(index, index + 2), 16));
  }
  return bytes;
}

function mergeRecords(records) {
  const output = [];
  for (const record of records) {
    const previous = output.at(-1);
    if (previous && previous.address + previous.bytes.length === record.address) {
      const combined = new Uint8Array(previous.bytes.length + record.bytes.length);
      combined.set(previous.bytes, 0);
      combined.set(record.bytes, previous.bytes.length);
      previous.bytes = combined;
      continue;
    }

    if (previous && record.address < previous.address + previous.bytes.length) {
      throw new IntelHexError("Intel HEX data records overlap.");
    }

    output.push({ address: record.address, bytes: record.bytes });
  }
  return output;
}
