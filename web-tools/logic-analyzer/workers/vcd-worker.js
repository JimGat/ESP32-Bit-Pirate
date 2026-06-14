import { writeVcd } from "../src/VcdWriter.js";

self.addEventListener("message", (event) => {
  try {
    const capture = event.data.capture;
    capture.packedSamples = new Uint8Array(capture.packedSamples);
    const started = performance.now();
    const vcd = writeVcd(capture);
    self.postMessage({
      type: "vcd-ready",
      vcd,
      elapsedMs: performance.now() - started,
      bytes: new TextEncoder().encode(vcd).length,
    });
  } catch (error) {
    self.postMessage({
      type: "vcd-error",
      message: error instanceof Error ? error.message : String(error),
    });
  }
});
