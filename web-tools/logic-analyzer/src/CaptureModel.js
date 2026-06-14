export function createDefaultChannels(count = 8, gpioMap = []) {
  return Array.from({ length: count }, (_, index) => ({
    index,
    bit: index,
    gpio: gpioMap[index] ?? null,
    name: `D${index}`,
    role: "",
    trigger: "none",
    enabled: true,
  }));
}

export function normalizeSamplesNewestFirst(samples) {
  const normalized = new Uint8Array(samples.length);
  for (let index = 0; index < samples.length; index += 1) {
    normalized[index] = samples[samples.length - 1 - index];
  }
  return normalized;
}

export function createNormalizedCapture({
  sampleRateHz,
  requestedSampleRateHz,
  sampleCount,
  triggerIndex = null,
  channels,
  packedSamples,
  startedAt = new Date().toISOString(),
  duration = null,
  deviceMetadata = {},
  config = {},
}) {
  return {
    sampleRateHz,
    requestedSampleRateHz,
    sampleCount,
    triggerIndex,
    channels,
    packedSamples,
    startedAt,
    duration,
    deviceMetadata,
    config,
  };
}

export function samplesToCsv(capture) {
  const enabledChannels = capture.channels.filter((channel) => channel.enabled);
  const header = ["sample", "time_s", ...enabledChannels.map((channel) => channel.name)];
  const lines = [header.join(",")];

  for (let index = 0; index < capture.sampleCount; index += 1) {
    const sample = capture.packedSamples[index] ?? 0;
    const row = [
      index,
      (index / capture.sampleRateHz).toPrecision(12),
      ...enabledChannels.map((channel) => String((sample >> channel.bit) & 1)),
    ];
    lines.push(row.join(","));
  }

  return `${lines.join("\n")}\n`;
}

export function captureToSessionJson(capture) {
  const sampleBase64 = bytesToBase64(capture.packedSamples);
  return JSON.stringify({
    version: 1,
    sampleRateHz: capture.sampleRateHz,
    requestedSampleRateHz: capture.requestedSampleRateHz,
    sampleCount: capture.sampleCount,
    triggerIndex: capture.triggerIndex,
    channels: capture.channels,
    startedAt: capture.startedAt,
    duration: capture.duration,
    deviceMetadata: capture.deviceMetadata,
    config: capture.config,
    packedSamplesBase64: sampleBase64,
  }, null, 2);
}

function bytesToBase64(bytes) {
  let binary = "";
  const chunkSize = 0x8000;
  for (let offset = 0; offset < bytes.length; offset += chunkSize) {
    binary += String.fromCharCode(...bytes.subarray(offset, offset + chunkSize));
  }
  return btoa(binary);
}
