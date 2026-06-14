import { SerialTransport } from "../../shared/serial/SerialTransport.js";
import { parseSumpMetadata } from "./SumpMetadataParser.js";
import { createDefaultChannels, createNormalizedCapture, normalizeSamplesNewestFirst } from "./CaptureModel.js";

export const SUMP = {
  RESET: 0x00,
  RUN: 0x01,
  ID: 0x02,
  METADATA: 0x04,
  SET_DIVIDER: 0x80,
  SET_READ_DELAY: 0x81,
  SET_FLAGS: 0x82,
};

const SUMP_METADATA_STRING_KEYS = new Set([0x01, 0x02, 0x03]);
const SUMP_METADATA_UINT32_KEYS = new Set([0x20, 0x21, 0x22, 0x23, 0x24]);

export const SUMP_COMPAT_PROFILE = {
  deviceName: "SUMP-compatible analyzer",
  firmwareVersion: "Compatibility profile",
  probeCount: 8,
  sampleMemoryBytes: 128 * 1024,
  maxSampleRateHz: 200000000,
  protocolVersion: 2,
  clockHz: 100000000,
};

export const ESP32_BP_COMPAT_PROFILE = SUMP_COMPAT_PROFILE;

export class SumpClient {
  constructor({ transport = new SerialTransport({ baudRate: 115200, bufferSize: 262144 }), log = () => {} } = {}) {
    this.transport = transport;
    this.log = log;
    this.metadata = null;
    this.id = null;
    this.captureCancelled = false;
  }

  async connect() {
    await this.transport.requestAndOpen();
    await this.resetParser();
    this.id = await this.identify();
    this.metadata = await this.readMetadata();
    return { id: this.id, metadata: this.metadata };
  }

  async disconnect() {
    await this.transport.close();
  }

  async resetParser() {
    this.logCommand("reset x5", [SUMP.RESET, SUMP.RESET, SUMP.RESET, SUMP.RESET, SUMP.RESET]);
    await this.transport.write(new Uint8Array([SUMP.RESET, SUMP.RESET, SUMP.RESET, SUMP.RESET, SUMP.RESET]));
    await delay(80);
    const stale = await this.transport.readAvailable(30);
    if (stale.length) {
      this.log(`Drained ${stale.length} stale byte(s) after reset.`);
    }
  }

  async identify() {
    this.logCommand("id", [SUMP.ID]);
    await this.transport.write(new Uint8Array([SUMP.ID]));
    const response = await this.transport.readExact(4, 1500);
    const text = new TextDecoder("ascii").decode(response);
    if (text !== "1ALS") {
      throw new Error("The selected serial port does not appear to be a SUMP logic analyzer.");
    }
    return { raw: text, deviceId: "SUMP Logic Analyzer", protocolMarker: "1ALS" };
  }

  async readMetadata() {
    this.logCommand("metadata", [SUMP.METADATA]);
    await this.transport.write(new Uint8Array([SUMP.METADATA]));

    try {
      const bytes = await this.readMetadataResponse(2048, 1500);
      const metadata = parseSumpMetadata(bytes);
      return { ...SUMP_COMPAT_PROFILE, ...metadata, clockHz: SUMP_COMPAT_PROFILE.clockHz };
    } catch (error) {
      this.log(`Metadata unavailable, using a conservative SUMP compatibility profile: ${error.message}`);
      return { ...SUMP_COMPAT_PROFILE, metadataFallback: true };
    }
  }

  async capture({ sampleRateHz = 1000000, sampleCount = 4096, channels = null, trigger = null, onProgress = () => {} } = {}) {
    if (trigger?.type && trigger.type !== "none") {
      throw new Error("Hardware trigger commands are not supported by this adapter firmware.");
    }

    this.captureCancelled = false;
    const metadata = this.metadata ?? ESP32_BP_COMPAT_PROFILE;
    const divider = calculateDivider(sampleRateHz, metadata.clockHz);
    const effectiveSampleRateHz = calculateEffectiveSampleRate(metadata.clockHz, divider);
    const safeSampleCount = clampSampleCount(sampleCount, metadata.sampleMemoryBytes);
    const readDelay = calculateReadDelayCounts(safeSampleCount, 0);

    await this.transport.write(commandWithLe32(SUMP.SET_DIVIDER, divider));
    this.logCommand("set divider", commandWithLe32(SUMP.SET_DIVIDER, divider));

    await this.transport.write(commandWithLe16Pair(SUMP.SET_READ_DELAY, readDelay.readCount, readDelay.delayCount));
    this.logCommand("set read/delay", commandWithLe16Pair(SUMP.SET_READ_DELAY, readDelay.readCount, readDelay.delayCount));

    await this.transport.write(commandWithLe32(SUMP.SET_FLAGS, 0));
    this.logCommand("set flags", commandWithLe32(SUMP.SET_FLAGS, 0));

    const startedAt = new Date().toISOString();
    const startMs = performance.now();
    await this.transport.write(new Uint8Array([SUMP.RUN]));
    this.logCommand("run", [SUMP.RUN]);

    const expectedBytes = safeSampleCount;
    const raw = await this.readExactWithProgress(expectedBytes, 10000, onProgress);
    const duration = performance.now() - startMs;
    const chronologicalSamples = normalizeSamplesNewestFirst(raw);
    const captureChannels = channels ?? createDefaultChannels(metadata.probeCount ?? 8);

    return createNormalizedCapture({
      sampleRateHz: effectiveSampleRateHz,
      requestedSampleRateHz: sampleRateHz,
      sampleCount: safeSampleCount,
      triggerIndex: null,
      channels: captureChannels,
      packedSamples: chronologicalSamples,
      startedAt,
      duration,
      deviceMetadata: metadata,
      config: { divider, readCount: readDelay.readCount, delayCount: readDelay.delayCount, expectedBytes },
    });
  }


  async cancelCapture() {
    this.captureCancelled = true;
    const error = createAbortError();
    this.transport.rejectWaiters?.(error);

    try {
      await this.transport.write(new Uint8Array([SUMP.RESET, SUMP.RESET, SUMP.RESET, SUMP.RESET, SUMP.RESET]));
      await delay(50);
      await this.transport.readAvailable(30);
    } catch (cancelError) {
      if (!this.captureCancelled) {
        throw cancelError;
      }
    }
  }

  async readMetadataResponse(maxBytes, timeoutMs) {
    const bytes = [];
    while (bytes.length < maxBytes) {
      const key = await this.transport.readByte(timeoutMs);
      bytes.push(key);

      if (key === 0x00) {
        return new Uint8Array(bytes);
      }

      if (SUMP_METADATA_STRING_KEYS.has(key) || !isKnownFixedSizeMetadataKey(key)) {
        await this.readMetadataStringValue(bytes, maxBytes, timeoutMs);
        continue;
      }

      const valueLength = key >= 0x40 && key <= 0x5f ? 1 : 4;
      for (let index = 0; index < valueLength; index += 1) {
        if (bytes.length >= maxBytes) {
          throw new Error("SUMP metadata response is too large.");
        }
        bytes.push(await this.transport.readByte(timeoutMs));
      }
    }
    throw new Error("SUMP metadata response is too large.");
  }

  async readMetadataStringValue(bytes, maxBytes, timeoutMs) {
    while (bytes.length < maxBytes) {
      const byte = await this.transport.readByte(timeoutMs);
      bytes.push(byte);
      if (byte === 0x00) {
        return;
      }
    }
    throw new Error("SUMP metadata response is too large.");
  }

  async readExactWithProgress(length, timeoutMs, onProgress) {
    const output = new Uint8Array(length);
    for (let offset = 0; offset < length; offset += 1) {
      if (this.captureCancelled) {
        throw createAbortError();
      }

      try {
        output[offset] = await this.transport.readByte(timeoutMs);
      } catch (error) {
        if (this.captureCancelled) {
          throw createAbortError();
        }
        throw error;
      }

      if ((offset & 0x3ff) === 0 || offset + 1 === length) {
        onProgress(offset + 1, length);
      }
    }
    return output;
  }

  logCommand(name, bytes) {
    this.log(`SUMP ${name}: ${formatHex(bytes)}`);
  }
}

export function calculateDivider(sampleRateHz, clockHz = SUMP_COMPAT_PROFILE.clockHz) {
  if (!Number.isFinite(sampleRateHz) || sampleRateHz <= 0) {
    throw new Error("Invalid sample rate.");
  }
  return Math.max(0, Math.floor(clockHz / sampleRateHz) - 1);
}

export function calculateEffectiveSampleRate(clockHz, divider) {
  return Math.floor(clockHz / (divider + 1));
}

export function calculateReadDelayCounts(sampleCount, delaySamples = 0) {
  if (sampleCount < 4) {
    throw new Error("SUMP captures require at least 4 samples.");
  }
  return {
    readCount: Math.ceil(sampleCount / 4) - 1,
    delayCount: Math.max(0, Math.ceil(delaySamples / 4)),
  };
}

export function clampSampleCount(sampleCount, sampleMemoryBytes) {
  const requested = Math.max(4, Math.floor(sampleCount / 4) * 4);
  const max = Math.max(4, Math.floor((sampleMemoryBytes || requested) / 4) * 4);
  return Math.min(requested, max);
}

function commandWithLe32(command, value) {
  const bytes = new Uint8Array(5);
  bytes[0] = command;
  bytes[1] = value & 0xff;
  bytes[2] = (value >> 8) & 0xff;
  bytes[3] = (value >> 16) & 0xff;
  bytes[4] = (value >> 24) & 0xff;
  return bytes;
}

function commandWithLe16Pair(command, first, second) {
  return new Uint8Array([
    command,
    first & 0xff,
    (first >> 8) & 0xff,
    second & 0xff,
    (second >> 8) & 0xff,
  ]);
}

function isKnownFixedSizeMetadataKey(key) {
  return SUMP_METADATA_UINT32_KEYS.has(key) || (key >= 0x40 && key <= 0x7f);
}

function formatHex(bytes) {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join(" ");
}

function createAbortError() {
  const error = new Error("Capture cancelled.");
  error.name = "AbortError";
  return error;
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
