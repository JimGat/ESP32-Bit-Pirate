const KB = 1024;
const MB = 1024 * KB;
const FLASH_START = 0x08000000;

const DEVICES = new Map([
  [0x0410, device("STM32F101/102/103 medium-density", "STM32F1", 128 * KB, uniform(1 * KB), 0x1ffff7e0)],
  [0x0412, device("STM32F101/102/103 low-density", "STM32F1", 32 * KB, uniform(1 * KB), 0x1ffff7e0)],
  [0x0414, device("STM32F101/103 high-density", "STM32F1", 512 * KB, uniform(2 * KB), 0x1ffff7e0)],
  [0x0418, device("STM32F105/107 connectivity line", "STM32F1", 256 * KB, uniform(2 * KB), 0x1ffff7e0)],
  [0x0420, device("STM32F100 medium-density value line", "STM32F1", 128 * KB, uniform(1 * KB), 0x1ffff7e0)],
  [0x0428, device("STM32F100 high-density value line", "STM32F1", 512 * KB, uniform(2 * KB), 0x1ffff7e0)],
  [0x0430, device("STM32F101/103 XL-density", "STM32F1", 1024 * KB, uniform(2 * KB), 0x1ffff7e0)],

  [0x0413, device("STM32F405/407/415/417", "STM32F4", 1024 * KB, sectors([
    16 * KB, 16 * KB, 16 * KB, 16 * KB, 64 * KB,
    128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB,
  ]), 0x1fff7a22)],
  [0x0419, device("STM32F42x/43x", "STM32F4", 2 * MB, sectors([
    16 * KB, 16 * KB, 16 * KB, 16 * KB, 64 * KB,
    128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB,
    16 * KB, 16 * KB, 16 * KB, 16 * KB, 64 * KB,
    128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB,
  ]), 0x1fff7a22)],
  [0x0423, device("STM32F401xB/xC", "STM32F4", 256 * KB, sectors([
    16 * KB, 16 * KB, 16 * KB, 16 * KB, 64 * KB, 128 * KB,
  ]), 0x1fff7a22)],
  [0x0433, device("STM32F401xD/xE", "STM32F4", 512 * KB, sectors([
    16 * KB, 16 * KB, 16 * KB, 16 * KB, 64 * KB, 128 * KB, 128 * KB, 128 * KB,
  ]), 0x1fff7a22)],
  [0x0431, device("STM32F411", "STM32F4", 512 * KB, sectors([
    16 * KB, 16 * KB, 16 * KB, 16 * KB, 64 * KB, 128 * KB, 128 * KB, 128 * KB,
  ]), 0x1fff7a22)],
  [0x0434, device("STM32F469/479", "STM32F4", 2 * MB, sectors([
    16 * KB, 16 * KB, 16 * KB, 16 * KB, 64 * KB,
    128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB,
    16 * KB, 16 * KB, 16 * KB, 16 * KB, 64 * KB,
    128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB, 128 * KB,
  ]), 0x1fff7a22)],

  [0x0449, device("STM32F74x/75x", "STM32F7", 1024 * KB, sectors([
    32 * KB, 32 * KB, 32 * KB, 32 * KB, 128 * KB,
    256 * KB, 256 * KB, 256 * KB,
  ]), 0x1ff0f442)],
  [0x0451, device("STM32F76x/77x", "STM32F7", 2 * MB, sectors([
    32 * KB, 32 * KB, 32 * KB, 32 * KB, 128 * KB,
    256 * KB, 256 * KB, 256 * KB,
    32 * KB, 32 * KB, 32 * KB, 32 * KB, 128 * KB,
    256 * KB, 256 * KB, 256 * KB,
  ]), 0x1ff0f442)],

  [0x0460, device("STM32G0x1", "STM32G0", 128 * KB, uniform(2 * KB), 0x1fff75e0)],
  [0x0466, device("STM32G03x/G04x", "STM32G0", 64 * KB, uniform(2 * KB), 0x1fff75e0)],
  [0x0468, device("STM32G4x1", "STM32G4", 512 * KB, uniform(2 * KB), 0x1fff75e0)],
  [0x0469, device("STM32G47x/G48x", "STM32G4", 512 * KB, uniform(2 * KB), 0x1fff75e0)],
  [0x0461, device("STM32L4x1/L4x2", "STM32L4", 256 * KB, uniform(2 * KB), 0x1fff75e0)],
  [0x0435, device("STM32L43x/L44x", "STM32L4", 256 * KB, uniform(2 * KB), 0x1fff75e0)],
  [0x0462, device("STM32L45x/L46x", "STM32L4", 512 * KB, uniform(2 * KB), 0x1fff75e0)],
]);

export function lookupDevice(deviceId) {
  const normalized = Number(deviceId) & 0xffff;
  const entry = DEVICES.get(normalized);
  if (entry) {
    return { ...entry, deviceId: normalized, known: true };
  }

  return {
    deviceId: normalized,
    known: false,
    name: `Unknown STM32 device 0x${normalized.toString(16).padStart(3, "0").toUpperCase()}`,
    family: "STM32",
    flashStart: FLASH_START,
    defaultFlashSize: 128 * KB,
    maxFlashSize: 16 * MB,
    flashSizeAddress: null,
    geometry: null,
  };
}

export function getEraseUnits(device, flashSize = device?.defaultFlashSize) {
  const start = device?.flashStart ?? FLASH_START;
  const size = normalizeFlashSize(flashSize, device?.defaultFlashSize ?? 128 * KB);
  const geometry = device?.geometry;
  if (!geometry || size <= 0) {
    return [];
  }

  if (geometry.type === "uniform") {
    const units = [];
    const count = Math.ceil(size / geometry.pageSize);
    for (let index = 0; index < count; index += 1) {
      const unitStart = start + index * geometry.pageSize;
      units.push({
        index,
        start: unitStart,
        size: Math.min(geometry.pageSize, start + size - unitStart),
        label: `Page ${index}`,
      });
    }
    return units;
  }

  if (geometry.type === "sectors") {
    const units = [];
    let cursor = start;
    for (let index = 0; index < geometry.sizes.length && cursor < start + size; index += 1) {
      const unitSize = Math.min(geometry.sizes[index], start + size - cursor);
      units.push({ index, start: cursor, size: unitSize, label: `Sector ${index}` });
      cursor += geometry.sizes[index];
    }
    return units;
  }

  return [];
}

export function getEraseUnitsForRange(device, flashSize, address, length) {
  if (!Number.isFinite(address) || !Number.isFinite(length) || length <= 0) {
    return [];
  }
  const end = address + length;
  return getEraseUnits(device, flashSize).filter((unit) => unit.start < end && unit.start + unit.size > address);
}

export function normalizeDetectedFlashSize(kilobytes, device) {
  const bytes = Number(kilobytes) * KB;
  if (!Number.isFinite(bytes) || bytes < 8 * KB || bytes > 16 * MB) {
    return device.defaultFlashSize;
  }
  return bytes;
}

export function formatDeviceId(deviceId) {
  return `0x${(Number(deviceId) & 0xffff).toString(16).padStart(4, "0").toUpperCase()}`;
}

function device(name, family, defaultFlashSize, geometry, flashSizeAddress = null) {
  return {
    name,
    family,
    flashStart: FLASH_START,
    defaultFlashSize,
    maxFlashSize: defaultFlashSize,
    flashSizeAddress,
    geometry,
  };
}

function uniform(pageSize) {
  return { type: "uniform", pageSize };
}

function sectors(sizes) {
  return { type: "sectors", sizes };
}

function normalizeFlashSize(value, fallback) {
  return Number.isFinite(value) && value > 0 ? Math.floor(value) : fallback;
}

export const MEMORY_UNITS = { KB, MB, FLASH_START };
