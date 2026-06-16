export function alignLength(length, alignment = 4) {
  const value = Number(length);
  const boundary = Number(alignment);
  if (!Number.isInteger(value) || value < 0 || !Number.isInteger(boundary) || boundary <= 0) {
    return Number.NaN;
  }
  return Math.ceil(value / boundary) * boundary;
}

export function getProgrammedLength(length, padToWord = false) {
  return padToWord ? alignLength(length, 4) : Number(length);
}

export function getFlashBounds(flashStart, flashSize) {
  const start = Number(flashStart);
  const size = Number(flashSize);
  if (!Number.isInteger(start) || start < 0 || !Number.isInteger(size) || size <= 0) {
    return null;
  }
  const end = start + size;
  if (!Number.isSafeInteger(end) || end > 0x100000000) {
    return null;
  }
  return { start, size, end };
}

export function validateFlashRange(address, length, flashStart, flashSize, label = "Memory range") {
  const start = Number(address);
  const size = Number(length);
  if (!Number.isInteger(start) || start < 0) {
    return `${label} start address is invalid.`;
  }
  if (!Number.isInteger(size) || size <= 0) {
    return `${label} size is invalid.`;
  }

  const bounds = getFlashBounds(flashStart, flashSize);
  if (!bounds) {
    return "Target flash capacity is unavailable or invalid.";
  }

  const end = start + size;
  if (!Number.isSafeInteger(end) || end > 0x100000000) {
    return `${label} exceeds the 32-bit address space.`;
  }
  if (start < bounds.start || end > bounds.end) {
    return `${label} ${formatHex(start)}–${formatHex(end - 1)} is outside flash ${formatHex(bounds.start)}–${formatHex(bounds.end - 1)}.`;
  }
  return null;
}

export function getSegmentBounds(segment, padToWord = false) {
  const address = Number(segment?.address);
  const sourceLength = Number(segment?.bytes?.length ?? segment?.length);
  const programmedLength = getProgrammedLength(sourceLength, padToWord);
  if (!Number.isInteger(address) || !Number.isInteger(programmedLength)) return null;
  return {
    address,
    sourceLength,
    programmedLength,
    end: address + programmedLength,
  };
}

export function getSegmentsSummary(segments, padToWord = false) {
  const normalized = (segments ?? [])
    .map((segment) => getSegmentBounds(segment, padToWord))
    .filter(Boolean)
    .sort((a, b) => a.address - b.address);
  if (!normalized.length) {
    return { segments: [], sourceBytes: 0, programmedBytes: 0, start: null, end: null, span: 0, gapBytes: 0 };
  }
  const sourceBytes = normalized.reduce((sum, segment) => sum + segment.sourceLength, 0);
  const programmedBytes = normalized.reduce((sum, segment) => sum + segment.programmedLength, 0);
  const start = normalized[0].address;
  const end = Math.max(...normalized.map((segment) => segment.end));
  return {
    segments: normalized,
    sourceBytes,
    programmedBytes,
    start,
    end,
    span: end - start,
    gapBytes: Math.max(0, end - start - programmedBytes),
  };
}

function formatHex(value) {
  return `0x${(Number(value) >>> 0).toString(16).padStart(8, "0").toUpperCase()}`;
}
