import assert from "node:assert/strict";
import { parseSumpMetadata } from "../src/SumpMetadataParser.js";
import { SumpClient, calculateDivider, calculateEffectiveSampleRate, calculateReadDelayCounts, clampSampleCount } from "../src/SumpClient.js";
import { normalizeSamplesNewestFirst, createDefaultChannels, createNormalizedCapture } from "../src/CaptureModel.js";
import { writeVcd } from "../src/VcdWriter.js";
import {
  alignCaptureToTrigger,
  calculateTriggerSearchSampleCount,
  findTriggerIndex,
  formatTriggerSummary,
  getTriggerConditions,
} from "../src/TriggerEngine.js";

class FakeMetadataTransport {
  constructor(bytes) {
    this.bytes = bytes;
    this.offset = 0;
  }

  async write() {}

  async readByte() {
    if (this.offset >= this.bytes.length) {
      throw new Error("No more metadata bytes.");
    }
    const byte = this.bytes[this.offset];
    this.offset += 1;
    return byte;
  }
}


globalThis.performance ??= { now: () => Date.now() };

await test("SUMP divider calculation follows firmware formula", () => {
  assert.equal(calculateDivider(1000000, 100000000), 99);
  assert.equal(calculateEffectiveSampleRate(100000000, 99), 1000000);
  assert.equal(calculateDivider(200000000, 100000000), 0);
  assert.equal(calculateEffectiveSampleRate(100000000, 0), 100000000);
});

await test("SUMP read/delay count groups samples by four", () => {
  assert.deepEqual(calculateReadDelayCounts(4096, 0), { readCount: 1023, delayCount: 0 });
  assert.deepEqual(calculateReadDelayCounts(4097, 10), { readCount: 1024, delayCount: 3 });
  assert.equal(clampSampleCount(4097, 8192), 4096);
  assert.equal(clampSampleCount(999999, 8192), 8192);
});

await test("metadata parser reads known values and keeps unknown keys", () => {
  const bytes = new Uint8Array([
    0x01, ...ascii("ESP32-BP SUMP"), 0x00,
    0x02, ...ascii("Polling MVP"), 0x00,
    0x20, 0x00, 0x00, 0x00, 0x08,
    0x21, 0x00, 0x02, 0x00, 0x00,
    0x23, 0x0b, 0xeb, 0xc2, 0x00,
    0x61, 0x00, 0x00, 0x12, 0x34,
    0x00,
  ]);
  const metadata = parseSumpMetadata(bytes);
  assert.equal(metadata.deviceName, "ESP32-BP SUMP");
  assert.equal(metadata.firmwareVersion, "Polling MVP");
  assert.equal(metadata.probeCount, 8);
  assert.equal(metadata.sampleMemoryBytes, 131072);
  assert.equal(metadata.maxSampleRateHz, 200000000);
  assert.deepEqual(metadata.unknown, [{ key: 0x61, value: 0x1234 }]);
});

await test("SUMP client reads metadata through string nulls to the final terminator", async () => {
  const bytes = new Uint8Array([
    0x01, ...ascii("ESP32-BP SUMP"), 0x00,
    0x02, ...ascii("Polling MVP"), 0x00,
    0x20, 0x00, 0x00, 0x00, 0x08,
    0x21, 0x00, 0x02, 0x80, 0x00,
    0x23, 0x0b, 0xeb, 0xc2, 0x00,
    0x00,
  ]);
  const transport = new FakeMetadataTransport(bytes);
  const client = new SumpClient({ transport });
  const metadata = await client.readMetadata();

  assert.equal(metadata.deviceName, "ESP32-BP SUMP");
  assert.equal(metadata.firmwareVersion, "Polling MVP");
  assert.equal(metadata.sampleMemoryBytes, 163840);
  assert.equal(metadata.metadataFallback, undefined);
});

await test("newest-first SUMP upload is normalized to chronological order", () => {
  assert.deepEqual(Array.from(normalizeSamplesNewestFirst(new Uint8Array([4, 3, 2, 1]))), [1, 2, 3, 4]);
});

await test("bit mapping uses channel bit numbers directly", () => {
  const channels = createDefaultChannels(3);
  assert.deepEqual(channels.map((channel) => channel.bit), [0, 1, 2]);
});


await test("per-channel software triggers detect levels and edges", () => {
  const samples = new Uint8Array([0b00, 0b00, 0b01, 0b11, 0b10]);
  assert.equal(findTriggerIndex(samples, [{ bit: 0, type: "rising" }]), 2);
  assert.equal(findTriggerIndex(samples, [{ bit: 1, type: "rising" }]), 3);
  assert.equal(findTriggerIndex(samples, [{ bit: 0, type: "falling" }]), 4);
  assert.equal(findTriggerIndex(samples, [{ bit: 1, type: "high" }]), 3);
  assert.equal(findTriggerIndex(samples, [{ bit: 0, type: "edge" }]), 2);
});

await test("multiple channel trigger conditions use PulseView-style AND matching", () => {
  const samples = new Uint8Array([0b00, 0b01, 0b11, 0b10]);
  const conditions = [
    { bit: 1, type: "rising", name: "CLK" },
    { bit: 0, type: "high", name: "DATA" },
  ];
  assert.equal(findTriggerIndex(samples, conditions), 2);
  assert.equal(formatTriggerSummary(conditions), "CLK ↑ + DATA 1");
});

await test("disabled channels are excluded from trigger conditions", () => {
  const channels = [
    { index: 0, bit: 0, name: "D0", enabled: true, trigger: "rising" },
    { index: 1, bit: 1, name: "D1", enabled: false, trigger: "falling" },
    { index: 2, bit: 2, name: "D2", enabled: true, trigger: "none" },
  ];
  assert.deepEqual(getTriggerConditions(channels), [
    { channelIndex: 0, bit: 0, name: "D0", type: "rising" },
  ]);
});


await test("trigger search reserves a full post-trigger output window", () => {
  assert.equal(calculateTriggerSearchSampleCount(4096, 16384), 8192);
  assert.throws(
    () => calculateTriggerSearchSampleCount(12288, 16384),
    /limited to 8192 samples/,
  );

  const samples = new Uint8Array([0, 1, 0, 1, 0, 1]);
  assert.equal(findTriggerIndex(samples, [{ bit: 0, type: "rising" }], { maxIndex: 2 }), 1);
  assert.equal(findTriggerIndex(samples, [{ bit: 0, type: "rising" }], { maxIndex: 0 }), -1);
});

await test("triggered capture is sliced so T is the first displayed sample", () => {
  const source = createNormalizedCapture({
    sampleRateHz: 1000,
    requestedSampleRateHz: 1000,
    sampleCount: 8,
    channels: createDefaultChannels(1),
    packedSamples: new Uint8Array([0, 0, 1, 1, 0, 0, 1, 1]),
    startedAt: "2026-06-14T00:00:00.000Z",
  });
  const aligned = alignCaptureToTrigger(source, 2, 4);

  assert.equal(aligned.sampleCount, 4);
  assert.equal(aligned.triggerIndex, 0);
  assert.deepEqual(Array.from(aligned.packedSamples), [1, 1, 0, 0]);
  assert.equal(aligned.startedAt, "2026-06-14T00:00:00.002Z");
});

await test("VCD writer emits changes only", () => {
  const capture = createNormalizedCapture({
    sampleRateHz: 1000000,
    sampleCount: 4,
    channels: createDefaultChannels(2),
    packedSamples: new Uint8Array([0b00, 0b01, 0b01, 0b11]),
    startedAt: "2026-06-14T00:00:00.000Z",
  });
  const vcd = writeVcd(capture);
  assert.match(vcd, /\$timescale 1 ns \$end/);
  assert.match(vcd, /\$var wire 1 ! D0 \$end/);
  assert.match(vcd, /#1000\n1!/);
  assert.match(vcd, /#3000\n1#/);
  assert.equal((vcd.match(/#2000/g) || []).length, 0);
});

function ascii(value) {
  return Array.from(Buffer.from(value, "ascii"));
}

async function test(name, fn) {
  try {
    await fn();
    console.log(`ok - ${name}`);
  } catch (error) {
    console.error(`not ok - ${name}`);
    throw error;
  }
}
