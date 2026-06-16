import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import {
  Stm32Bootloader,
  STM32_COMMANDS,
  encodeAddress,
  encodeCommand,
  xorChecksum,
} from "../src/Stm32Bootloader.js";
import { parseIntelHex } from "../src/IntelHex.js";
import { getEraseUnits, getEraseUnitsForRange, lookupDevice } from "../src/Stm32DeviceDatabase.js";
import { StlinkWebUsbAdapter, createStlinkCompatibilityDevice } from "../src/StlinkWebUsbAdapter.js";

class MockTransport {
  constructor(bytes = []) {
    this.queue = [...bytes];
    this.writes = [];
    this.port = {};
  }

  async write(bytes) {
    this.writes.push(new Uint8Array(bytes));
  }

  async readByte() {
    if (!this.queue.length) throw new Error("Mock RX queue is empty");
    return this.queue.shift();
  }

  async readExact(length) {
    const output = new Uint8Array(length);
    for (let index = 0; index < length; index += 1) output[index] = await this.readByte();
    return output;
  }

  getInfo() {
    return {};
  }
}

function bytes(value) {
  return [...value];
}

const pageHtml = readFileSync(new URL("../index.html", import.meta.url), "utf8");
assert.match(pageHtml, /<option value="stlink" selected>ST-Link \(WebUSB\)<\/option>/);
assert.match(pageHtml, /<details class="content-card log-card" open>/);

assert.deepEqual(bytes(encodeCommand(0x31)), [0x31, 0xce]);
assert.deepEqual(bytes(encodeAddress(0x08000000)), [0x08, 0x00, 0x00, 0x00, 0x08]);
assert.equal(xorChecksum(Uint8Array.of(0x03, 0x01, 0x02, 0x03, 0xff)), 0xfc);

const parsed = parseIntelHex([
  ":020000040800F2",
  ":0400000001020304F2",
  ":0400000508000000EF",
  ":00000001FF",
].join("\n"));
assert.equal(parsed.segments.length, 1);
assert.equal(parsed.segments[0].address, 0x08000000);
assert.deepEqual(bytes(parsed.segments[0].bytes), [1, 2, 3, 4]);
assert.equal(parsed.startAddress, 0x08000000);

const f103 = lookupDevice(0x0410);
assert.equal(f103.known, true);
assert.equal(getEraseUnits(f103, 128 * 1024).length, 128);
const f103Range = getEraseUnitsForRange(f103, 128 * 1024, 0x08000300, 0x600);
assert.deepEqual(f103Range.map((unit) => unit.index), [0, 1, 2]);

{
  const transport = new MockTransport([0x79, 0x79, 0x79]);
  const loader = new Stm32Bootloader({ transport });
  loader.commands = new Set([STM32_COMMANDS.WRITE_MEMORY]);
  await loader.writeMemory(0x08000000, Uint8Array.of(1, 2, 3));
  assert.deepEqual(bytes(transport.writes[0]), [0x31, 0xce]);
  assert.deepEqual(bytes(transport.writes[1]), [0x08, 0x00, 0x00, 0x00, 0x08]);
  assert.deepEqual(bytes(transport.writes[2]), [0x03, 0x01, 0x02, 0x03, 0xff, 0xfc]);
}

{
  const transport = new MockTransport([0x79, 0x79, 0x79, 0xaa, 0xbb, 0xcc]);
  const loader = new Stm32Bootloader({ transport });
  loader.commands = new Set([STM32_COMMANDS.READ_MEMORY]);
  const output = await loader.readMemory(0x08000000, 3);
  assert.deepEqual(bytes(output), [0xaa, 0xbb, 0xcc]);
  assert.deepEqual(bytes(transport.writes[2]), [0x02, 0xfd]);
}

{
  const transport = new MockTransport([0x79, 0x79]);
  const loader = new Stm32Bootloader({ transport });
  loader.commands = new Set([STM32_COMMANDS.EXTENDED_ERASE]);
  await loader.massErase();
  assert.deepEqual(bytes(transport.writes[0]), [0x44, 0xbb]);
  assert.deepEqual(bytes(transport.writes[1]), [0xff, 0xff, 0x00]);
}


{
  const transport = new MockTransport([0x79, 0x31, 0xaa, 0x55, 0x79]);
  const loader = new Stm32Bootloader({ transport });
  loader.commands = new Set([STM32_COMMANDS.GET_VERSION]);
  const info = await loader.getVersionAndProtection();
  assert.equal(info.version, 0x31);
  assert.deepEqual(bytes(info.optionBytes), [0xaa, 0x55]);
  assert.deepEqual(bytes(transport.writes[0]), [0x01, 0xfe]);
}

{
  const transport = new MockTransport([0x79, 0x79]);
  const loader = new Stm32Bootloader({ transport });
  loader.commands = new Set([STM32_COMMANDS.EXTENDED_ERASE_NO_STRETCH]);
  await loader.erasePages([0x0123, 0x0124]);
  assert.deepEqual(bytes(transport.writes[0]), [0x45, 0xba]);
  assert.deepEqual(bytes(transport.writes[1]), [0x00, 0x01, 0x01, 0x23, 0x01, 0x24, 0x06]);
}


{
  const originalNavigator = Object.getOwnPropertyDescriptor(globalThis, "navigator");
  Object.defineProperty(globalThis, "navigator", {
    configurable: true,
    value: { usb: {} },
  });

  const calls = [];
  const usbDevice = {
    vendorId: 0x0483,
    productId: 0x3748,
    productName: "ST-LINK/V2",
    serialNumber: "TEST123",
  };

  class MockWebStlink {
    constructor(logger) {
      this.logger = logger;
      this._stlink = { ver_str: "V2J99", target_voltage: 3.3 };
      this._mcus_by_devid = { erase_sizes: [1024] };
      this._driver = {
        flash_erase_all: async () => calls.push("massErase"),
      };
      this.debug = false;
      this.halted = false;
    }

    async attach(device) { calls.push(["attach", device]); }
    async detach() { calls.push("detach"); }
    async detect_cpu() {
      calls.push("detect");
      return {
        core: "Cortex-M3",
        dev_id: 0x410,
        type: "STM32F103xB",
        flash_size: 128,
        sram_size: 20,
        flash_start: 0x08000000,
      };
    }
    async inspect_cpu() { return { debug: this.debug, halted: this.halted }; }
    async set_debug_enable() { this.debug = true; calls.push("debug"); }
    async halt() { this.halted = true; calls.push("halt"); }
    async read_memory(address, size) {
      calls.push(["read", address, size]);
      this.logger.bargraph_start("Reading memory", 0, size);
      this.logger.bargraph_update(size);
      this.logger.bargraph_done();
      return Uint8Array.from({ length: size }, (_, index) => index & 0xff);
    }
    async flash(address, data) {
      calls.push(["flash", address, [...data]]);
      this.logger.bargraph_start("Erasing FLASH", { value_min: address, value_max: address + data.length });
      this.logger.bargraph_update({ value: address + data.length });
      this.logger.bargraph_done();
      this.logger.bargraph_start("Writing FLASH", { value_min: address, value_max: address + data.length });
      this.logger.bargraph_update({ value: address + Math.floor(data.length / 2) });
      this.logger.bargraph_update({ value: address + data.length });
      this.logger.bargraph_done();
    }
    async reset(halt) { this.halted = Boolean(halt); calls.push(["reset", halt]); }
    async run() { this.halted = false; calls.push("run"); }
  }

  let filters = null;
  const adapter = new StlinkWebUsbAdapter({
    requestDevice: async (options) => {
      filters = options.filters;
      return usbDevice;
    },
    moduleLoader: async () => MockWebStlink,
  });

  const info = await adapter.connect();
  assert.equal(filters[0].vendorId, 0x0483);
  assert.equal(filters[0].productId, 0x3748);
  assert.equal(info.connected, true);
  assert.equal(info.deviceId, 0x410);
  assert.equal(info.flashSize, 128 * 1024);
  assert.equal(info.portInfo.productName, "ST-LINK/V2");

  const progress = [];
  const data = await adapter.readMemory(0x08000000, 20, {
    chunkSize: 8,
    progress: ({ done }) => progress.push(done),
  });
  assert.equal(data.length, 20);
  assert.deepEqual(progress, [8, 16, 20]);

  const flashProgress = [];
  await adapter.flash(0x08000000, Uint8Array.of(1, 2, 3, 4), {
    progress: ({ done, total }) => flashProgress.push([Math.round(done), total]),
  });
  assert.ok(flashProgress.some(([done, total]) => done > 0 && done < total));
  assert.deepEqual(flashProgress.at(-1), [100, 100]);
  await adapter.massErase();
  await adapter.go();
  assert.ok(calls.some((entry) => Array.isArray(entry) && entry[0] === "flash"));
  assert.ok(calls.includes("massErase"));
  assert.ok(calls.includes("run"));

  await adapter.disconnect();
  assert.equal(adapter.connected, false);
  assert.ok(calls.includes("detach"));

  if (originalNavigator) Object.defineProperty(globalThis, "navigator", originalNavigator);
  else delete globalThis.navigator;
}


{
  let lastOut = new Uint8Array(0);
  const transferInLengths = [];
  const rawDevice = {
    async transferOut(_endpoint, data) {
      lastOut = new Uint8Array(data.buffer, data.byteOffset, data.byteLength).slice();
      return { status: "ok", bytesWritten: lastOut.length };
    },
    async transferIn(_endpoint, length) {
      transferInLengths.push(length);
      let response;
      if (lastOut[0] === 0xf1) {
        response = Uint8Array.of(0x29, 0x87, 0x83, 0x04, 0x48, 0x37);
      } else if (lastOut[0] === 0xf7) {
        response = Uint8Array.of(0xea, 0x05, 0x00, 0x00, 0xee, 0x07, 0x00, 0x00);
      } else if (lastOut[0] === 0xf2 && lastOut[1] === 0x31) {
        response = Uint8Array.of(0x80, 0x00, 0x00, 0x00, 0x77, 0x14, 0xa0, 0x2b, 0x00, 0x00, 0x00, 0x00);
      } else {
        response = new Uint8Array(length);
      }
      return {
        status: "ok",
        data: new DataView(response.buffer, response.byteOffset, response.byteLength),
      };
    },
  };

  const logs = [];
  const compatibility = createStlinkCompatibilityDevice(rawDevice, {
    swdDivisor: 7,
    swdSpeedLabel: "480 kHz",
    log: (message) => logs.push(message),
  });

  await compatibility.device.transferOut(1, Uint8Array.of(0xf1, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
  await compatibility.device.transferIn(1, 6);
  assert.equal(compatibility.state.apiVersion, 2);

  await compatibility.device.transferOut(1, Uint8Array.of(0xf7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
  await compatibility.device.transferIn(1, 8);
  assert.ok(compatibility.state.targetVoltage > 3.2 && compatibility.state.targetVoltage < 3.4);

  await compatibility.device.transferOut(1, Uint8Array.of(0xf2, 0x43, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
  assert.deepEqual([...lastOut.slice(0, 3)], [0xf2, 0x43, 0x07]);

  await compatibility.device.transferOut(1, Uint8Array.of(0xf2, 0x22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
  assert.deepEqual([...lastOut.slice(0, 2)], [0xf2, 0x31]);
  const coreIdResponse = await compatibility.device.transferIn(1, 4);
  assert.equal(transferInLengths.at(-1), 12);
  assert.equal(coreIdResponse.data.byteLength, 4);
  assert.equal(coreIdResponse.data.getUint32(0, true), 0x2ba01477);
  assert.ok(logs.some((message) => message.includes("API-v2")));
}

console.log("STM32 Web Tool tests passed.");
