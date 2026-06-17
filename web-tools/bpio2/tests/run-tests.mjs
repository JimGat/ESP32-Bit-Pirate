import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import {
  FlatBufferWriter,
  RequestType,
  ResponseType,
  buildConfigurationRequest,
  buildDataRequest,
  buildStatusRequest,
  cobsDecode,
  cobsEncode,
  openRootTable,
  parseConfigurationResponse,
  parseDataResponse,
  parseStatusResponse,
} from "../src/Bpio2Codec.js";

for (const length of [0, 1, 2, 253, 254, 255, 512, 4096]) {
  const input = Uint8Array.from({ length }, (_, index) => (index * 37) & 0xff);
  const decoded = cobsDecode(cobsEncode(input));
  assert.deepEqual([...decoded], [...input], `COBS round trip ${length}`);
}

{
  const request = buildStatusRequest();
  const root = openRootTable(request);
  assert.equal(root.getU8(0), 2);
  assert.equal(root.getU16(1), 2);
  assert.equal(root.getU8(2), RequestType.STATUS);
  assert.deepEqual([...root.getTable(3).getByteVector(0)], [0]);
}

{
  const request = buildConfigurationRequest({
    mode: "SPI",
    bitOrderMsb: false,
    ioDirectionMask: 0xf0,
    ioDirection: 0x10,
    modeConfiguration: {
      speed: 10_000_000,
      dataBits: 8,
      clockPolarity: true,
      clockPhase: false,
      chipSelectIdle: true,
    },
  });
  const root = openRootTable(request);
  assert.equal(root.getU8(2), RequestType.CONFIGURATION);
  const config = root.getTable(3);
  assert.equal(config.getString(0), "SPI");
  assert.equal(config.getBool(3), true);
  assert.equal(config.getU8(10), 0xf0);
  assert.equal(config.getU8(11), 0x10);
  const mode = config.getTable(1);
  assert.equal(mode.getU32(0), 10_000_000);
  assert.equal(mode.getBool(7), true);
  assert.equal(mode.getBool(8), false);
  assert.equal(mode.getBool(9), true);
}

{
  const request = buildDataRequest({
    startAlt: true,
    dataWrite: Uint8Array.of(0x9f, 0xaa),
    bytesRead: 3,
    stopMain: true,
  });
  const root = openRootTable(request);
  assert.equal(root.getU8(2), RequestType.DATA);
  const data = root.getTable(3);
  assert.equal(data.getBool(1), true);
  assert.deepEqual([...data.getByteVector(2)], [0x9f, 0xaa]);
  assert.equal(data.getU16(3), 3);
  assert.equal(data.getBool(4), true);
}

{
  const writer = new FlatBufferWriter(2048);
  const root = writer.createRootTable(3);
  writer.setU8(root, 1, ResponseType.STATUS);
  const status = writer.createTable(29);
  writer.setOffset(root, 2, status.object);
  writer.setU8(status, 1, 2);
  writer.setU16(status, 2, 2);
  writer.setU8(status, 5, 1);
  writer.setU8(status, 6, 6);
  writer.setU32(status, 13, 33024);
  writer.setU32(status, 14, 32768);
  writer.setU32(status, 15, 32768);
  writer.setU8(status, 24, 0x07);
  writer.setU8(status, 25, 0xb0);
  writer.setFloat32(status, 26, 12.5);
  const modes = writer.createStringVector(["HiZ", "SPI", "I2C"]);
  writer.setOffset(status, 9, modes);
  const current = writer.createString("SPI");
  writer.setOffset(status, 10, current);
  const pins = writer.createStringVector(["CS GPIO 5", "CLK GPIO 15", "MOSI GPIO 39", "MISO GPIO 13"]);
  writer.setOffset(status, 11, pins);

  const parsed = parseStatusResponse(writer.output());
  assert.equal(parsed.versionFlatbuffersMajor, 2);
  assert.equal(parsed.versionFirmwareMajor, 1);
  assert.equal(parsed.modeCurrent, "SPI");
  assert.deepEqual(parsed.modesAvailable, ["HiZ", "SPI", "I2C"]);
  assert.deepEqual(parsed.modePinLabels, ["CS GPIO 5", "CLK GPIO 15", "MOSI GPIO 39", "MISO GPIO 13"]);
  assert.equal(parsed.modeMaxRead, 32768);
  assert.equal(parsed.ioValue, 0xb0);
  assert.equal(parsed.diskSizeMb, 12.5);
}

{
  const writer = new FlatBufferWriter(256);
  const root = writer.createRootTable(3);
  writer.setU8(root, 1, ResponseType.CONFIGURATION);
  const response = writer.createTable(1);
  writer.setOffset(root, 2, response.object);
  assert.equal(parseConfigurationResponse(writer.output()).error, "");
}

{
  const writer = new FlatBufferWriter(256);
  const root = writer.createRootTable(3);
  writer.setU8(root, 1, ResponseType.DATA);
  const response = writer.createTable(3);
  writer.setOffset(root, 2, response.object);
  const vector = writer.createByteVector([0xef, 0x40, 0x17]);
  writer.setOffset(response, 1, vector);
  const parsed = parseDataResponse(writer.output());
  assert.equal(parsed.error, "");
  assert.deepEqual([...parsed.data], [0xef, 0x40, 0x17]);
}

{
  const appSource = readFileSync(new URL("../src/App.js", import.meta.url), "utf8");
  assert.match(appSource, /renderStatus\(\{ renderGpio: false \}\)/, "live polling must not rebuild GPIO cards");
  assert.match(appSource, /direction\.value = "output"/, "level changes must promote the pin to output");
  assert.match(appSource, /gpioTraceSamples = Array\.from\(\{ length: 8 \}/, "all eight GPIO traces must retain samples");
  assert.match(appSource, /sampleAllGpioTraces\(status\.ioValue\)/, "every live status response must sample all pins");
  assert.match(appSource, /updateAllLogicTraces\(\)/, "all visible logic traces must update together");
  assert.doesNotMatch(appSource, /data-logic-pin|handleDocumentLogicDismiss|stopLogicAnalysis/, "per-pin Logic buttons and dismissal logic must stay removed");
  assert.match(appSource, /gpioRequestedState = new Map\(\)/, "GPIO output commands must retain their requested state");
  assert.doesNotMatch(appSource, /is-reserved|LOCKED|reserved by/, "GPIO bus pins must remain fully controllable");
  assert.match(appSource, /async function runOperation[\s\S]*?if \(busy\) return;/, "live GPIO polling must not lock commands");
  assert.match(appSource, /name === "gpio"[\s\S]*startLivePinMapping/, "opening GPIO must start live monitoring");
  assert.match(appSource, /value >= 5 && value <= 1000/, "live sampling must support 5 ms through 1 second");
  assert.match(appSource, /await configureSpiFromUi\(\);[\s\S]*?client\.spiTransfer/, "sequence SPI steps must restore SPI before transfer");
  assert.match(appSource, /await configureI2cFromUi\(\);[\s\S]*?client\.i2cTransfer/, "sequence I2C steps must restore I2C before transfer");
  assert.match(appSource, /step\.unit === "us"[\s\S]*delayMicroseconds\(step\.value\)/, "microsecond sequence delays must use a browser busy-wait");
  assert.match(appSource, /await delayMilliseconds\(step\.value\)/, "millisecond sequence delays must remain non-blocking");
  assert.match(appSource, /while \(performance\.now\(\) - start < durationMs\)/, "microsecond delays must use the highest-resolution browser clock available");
  assert.match(appSource, /microseconds \? "100000" : "60000"/, "sequence delay limit must follow the selected unit");
  assert.match(appSource, /\["I2C SCL", "SPI SCK"\]/, "IO1 must expose permanent I2C and SPI role tags");
  assert.match(appSource, /probeModeLimits/, "SPI and I2C limits must be detected independently of HiZ");
  assert.match(appSource, /directionMask: allPinsMask,\s*direction: 0/, "HiZ release must explicitly configure every pin as input");
  assert.match(appSource, /status = applyRequestedGpioState\(nextStatus, 0xff\)/, "live polling must preserve explicitly requested input directions");

  const htmlSource = readFileSync(new URL("../index.html", import.meta.url), "utf8");
  assert.doesNotMatch(htmlSource, /Read JEDEC ID/, "the redundant JEDEC shortcut must stay removed");
  assert.equal((htmlSource.match(/class="tab-icon"/g) ?? []).length, 4, "each operation tab must have an icon");
  assert.match(htmlSource, /operation-tab is-active[\s\S]*data-operation-tab="gpio"/, "GPIO must be the default first tab");
  assert.match(htmlSource, /data-operation-tab="sequence"[\s\S]*<\/div>\s*<div class="operation-panels">/, "Sequence must be the final tab");
  assert.match(htmlSource, /option value="5">5 ms<\/option>/, "refresh selector must include 5 ms");
  assert.match(htmlSource, /option value="1000">1 s<\/option>/, "refresh selector must include 1 second");
  assert.match(htmlSource, /id="sequenceDelayUnit"[\s\S]*option value="ms" selected>ms<\/option>[\s\S]*option value="us">µs<\/option>/, "sequence delays must support both milliseconds and microseconds");

  assert.match(htmlSource, /option value="10" selected>10 ms<\/option>/, "10 ms must be the default refresh delay");
  assert.doesNotMatch(htmlSource, /Every pin is sampled and displayed as a live logic trace/, "GPIO helper copy must remain concise");
  assert.doesNotMatch(htmlSource, /Probe standard 7-bit addresses/, "I2C scan helper must omit redundant probe wording");
  assert.match(appSource, /elements\.spiLimit\.textContent = formatSpiLimit\(modeLimits\.SPI\)/, "SPI limit must use a single-size formatter");
  assert.match(appSource, /elements\.i2cLimit\.textContent = formatSpiLimit\(modeLimits\.I2C\)/, "I2C limit must use a single-size formatter");
  assert.match(htmlSource, /All INPUT \(HiZ\)/, "the release action must clearly describe the resulting direction");
  assert.match(htmlSource, /id="spiCustomSpeed"[^>]*value="12\.5"[^>]*placeholder="e\.g\. 22"/, "custom SPI input must accept a plain MHz value");
  assert.match(htmlSource, /speed-custom-input[\s\S]*?>MHz<\/span>/, "custom SPI input must visibly identify MHz");
  assert.match(appSource, /parseMegahertz\(elements\.spiCustomSpeed\.value, 40, "SPI speed"\)/, "custom SPI speed must be interpreted in MHz");
  assert.match(appSource, /megahertz \* 1_000_000/, "custom MHz values must be converted to Hz before encoding");
  assert.doesNotMatch(appSource, /function parseFrequency/, "unit-based custom frequency parsing must stay removed");
}

console.log("BPIO2 codec and UI regression tests passed.");
