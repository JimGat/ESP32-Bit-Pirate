const FEATURED_PARTS = [
  { id: "m328p", name: "ATmega328P", signature: "0x1e950f", flash: 32768, eeprom: 1024, page: 128 },
  { id: "m168", name: "ATmega168", signature: "0x1e9406", flash: 16384, eeprom: 512, page: 128 },
  { id: "m8", name: "ATmega8", signature: "0x1e9307", flash: 8192, eeprom: 512, page: 64 },
  { id: "m32u4", name: "ATmega32U4", signature: "0x1e9587", flash: 32768, eeprom: 1024, page: 128 },
  { id: "m2560", name: "ATmega2560", signature: "0x1e9801", flash: 262144, eeprom: 4096, page: 256 },
  { id: "t85", name: "ATtiny85", signature: "0x1e930b", flash: 8192, eeprom: 512, page: 64 },
  { id: "t84", name: "ATtiny84", signature: "0x1e930c", flash: 8192, eeprom: 512, page: 64 },
  { id: "t13", name: "ATtiny13", signature: "0x1e9007", flash: 1024, eeprom: 64, page: 32 },
  { id: "t2313", name: "ATtiny2313", signature: "0x1e910a", flash: 2048, eeprom: 128, page: 32 },
];

export class TargetRepository {
  constructor({ confUrl = "wasm/avrdude.conf", log = () => {} } = {}) {
    this.confUrl = confUrl;
    this.log = log;
    this.parts = [...FEATURED_PARTS];
    this.loadedFromConf = false;
  }

  async load() {
    try {
      const response = await fetch(this.confUrl, { cache: "no-store" });
      if (!response.ok) {
        throw new Error(`${response.status} ${response.statusText}`);
      }
      const conf = await response.text();
      const parsed = parseParts(conf);
      this.parts = mergeFeatured(parsed);
      this.loadedFromConf = true;
      this.log(`Loaded ${this.parts.length} AVR part definition(s) from AVRDUDE config.`);
    } catch (error) {
      this.loadedFromConf = false;
      this.log(`AVRDUDE config not loaded yet; using featured part list. ${error.message}`);
    }
    return this.parts;
  }

  all() {
    return this.parts;
  }

  findByText(text) {
    const normalized = normalize(text);
    return this.parts.find((part) => normalize(part.name) === normalized || normalize(part.id) === normalized)
      ?? this.parts.find((part) => normalize(part.name).includes(normalized) || normalize(part.id).includes(normalized))
      ?? null;
  }

  findBySignature(signature) {
    const normalized = normalizeSignature(signature);
    return this.parts.find((part) => normalizeSignature(part.signature) === normalized) ?? null;
  }
}

function parseParts(conf) {
  const parts = [];
  const blocks = conf.split(/\n\s*part\s*\n/g).slice(1);
  for (const block of blocks) {
    const id = matchField(block, "id");
    if (!id) {
      continue;
    }
    parts.push({
      id,
      name: matchField(block, "desc") ?? id,
      signature: parseSignature(block),
      flash: parseMemorySize(block, "flash"),
      eeprom: parseMemorySize(block, "eeprom"),
      page: parsePageSize(block, "flash"),
    });
  }
  return parts;
}

function matchField(block, field) {
  const match = block.match(new RegExp(`${field}\\s*=\\s*"([^"]+)"\\s*;`));
  return match?.[1] ?? null;
}

function parseSignature(block) {
  const match = block.match(/signature\s*=\s*0x([0-9a-f]{2})\s+0x([0-9a-f]{2})\s+0x([0-9a-f]{2})\s*;/i);
  return match ? `0x${match[1]}${match[2]}${match[3]}`.toLowerCase() : null;
}

function parseMemorySize(block, memory) {
  const memoryBlock = memorySection(block, memory);
  const match = memoryBlock?.match(/size\s*=\s*([0-9]+)\s*;/);
  return match ? Number.parseInt(match[1], 10) : null;
}

function parsePageSize(block, memory) {
  const memoryBlock = memorySection(block, memory);
  const match = memoryBlock?.match(/page_size\s*=\s*([0-9]+)\s*;/);
  return match ? Number.parseInt(match[1], 10) : null;
}

function memorySection(block, memory) {
  const index = block.indexOf(`memory "${memory}"`);
  if (index === -1) {
    return null;
  }
  const rest = block.slice(index);
  const end = rest.indexOf("\n    ;");
  return end === -1 ? rest : rest.slice(0, end);
}

function mergeFeatured(parsed) {
  const byId = new Map(parsed.map((part) => [part.id, part]));
  for (const part of FEATURED_PARTS) {
    byId.set(part.id, { ...part, ...byId.get(part.id) });
  }
  const featured = FEATURED_PARTS.map((part) => byId.get(part.id));
  const remaining = [...byId.values()].filter((part) => !FEATURED_PARTS.some((featuredPart) => featuredPart.id === part.id));
  return [...featured, ...remaining];
}

function normalize(value) {
  return String(value ?? "").toLowerCase().replace(/[^a-z0-9]/g, "");
}

function normalizeSignature(value) {
  return String(value ?? "").toLowerCase().replace(/[^0-9a-f]/g, "");
}

export function formatSize(bytes) {
  if (!bytes) {
    return "-";
  }
  if (bytes >= 1024) {
    return `${bytes / 1024} KiB`;
  }
  return `${bytes} bytes`;
}
