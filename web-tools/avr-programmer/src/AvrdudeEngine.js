import { AvrdudeCommandBuilder, argsToString } from "./AvrdudeCommandBuilder.js";
import { parseSignature } from "./OutputParser.js";

const LOCAL_ARTIFACTS = {
  source: "local artifacts",
  module: new URL("../wasm/avrdude.js", import.meta.url).href,
  wasm: new URL("../wasm/avrdude.wasm", import.meta.url).href,
  conf: new URL("../wasm/avrdude.conf", import.meta.url).href,
  worker: new URL("../avrdude-worker.js", import.meta.url).href,
};

export class AvrdudeEngine {
  constructor({ log = () => {}, output = () => {} } = {}) {
    this.log = log;
    this.output = output;
    this.builder = new AvrdudeCommandBuilder();
    this.connected = false;
    this.selectedPort = null;
    this.lastOutput = "";
    this.artifacts = null;
    this.moduleFactory = null;
    this.configText = null;
    this.version = null;
  }

  async initialize() {
    if (this.moduleFactory && this.configText) {
      return { version: this.version };
    }

    this.assertRuntimeRequirements();
    this.artifacts = await this.resolveArtifacts();

    const moduleNamespace = await import(this.artifacts.module);
    const factory = moduleNamespace.default ?? moduleNamespace.Module ?? window.Module;
    if (typeof factory !== "function") {
      throw new Error("AVRDUDE WebAssembly module did not export an Emscripten factory.");
    }

    const confResponse = await fetch(this.artifacts.conf, { cache: "no-store", mode: "cors" });
    if (!confResponse.ok) {
      throw new Error(`Unable to load AVRDUDE configuration (${confResponse.status} ${confResponse.statusText}).`);
    }

    this.moduleFactory = factory;
    this.configText = await confResponse.text();
    this.version = this.parseConfigVersion(this.configText);

    // Do not execute `avrdude -?` here. This build is not safely reusable after
    // startAvrdude() exits, so every real operation receives a fresh WASM runtime.
    this.log(
      `AVRDUDE WASM prepared from ${this.artifacts.source}. `
      + `${this.version ? `Version ${this.version}.` : "Version not reported by avrdude.conf."}`,
    );

    return { version: this.version };
  }

  async connect(port) {
    if (!port) {
      throw new Error("No Web Serial port was selected.");
    }

    this.selectedPort = port;
    window.activePort = port;
    this.connected = true;
    this.log("Selected Web Serial port bound to AVRDUDE serial bridge.");
  }

  configureProgrammer(options) {
    this.builder.configure(options);
  }

  async disconnect() {
    const port = this.selectedPort;
    this.connected = false;
    this.selectedPort = null;
    window.activePort = null;
    window.funcs = null;

    if (port && (port.readable || port.writable)) {
      try {
        await port.close();
      } catch (error) {
        this.log(`Serial port close warning: ${this.formatDiagnostic(error)}`);
      }
    }

    if (window.avrDudeWorker) {
      try {
        window.avrDudeWorker.terminate();
      } catch {
        // Ignore a worker that already terminated itself.
      }
      window.avrDudeWorker = null;
    }

    this.log("AVRDUDE serial bridge disconnected.");
  }

  async detectPart(partId) {
    try {
      const result = await this.run(this.builder.detectSignature(partId));
      return { ...result, signature: parseSignature(result.output) };
    } catch (error) {
      const output = error?.avrdudeOutput ?? this.lastOutput ?? "";
      const signature = parseSignature(output);
      if (signature) {
        this.log(`AVR signature ${signature} recovered from AVRDUDE output despite a part mismatch.`);
        return {
          exitCode: error?.exitCode ?? 1,
          output,
          command: null,
          module: null,
          signature,
          partMismatch: true,
        };
      }
      throw error;
    }
  }

  async readMemory(partId, memory, format = "i") {
    const outputPath = `/tmp/${memory}-dump.${format === "r" ? "bin" : "hex"}`;
    const result = await this.run(this.builder.readMemory(partId, memory, outputPath, format));
    const data = result.module.FS.readFile(outputPath);
    return { ...result, data, outputPath };
  }

  async writeMemory(partId, memory, file, format = "i") {
    const inputPath = `/tmp/${memory}-input.${format === "r" ? "bin" : "hex"}`;
    const inputFile = await this.prepareInputFile(inputPath, file);
    return this.run(
      this.builder.writeMemory(partId, memory, inputPath, format),
      { inputFiles: [inputFile] },
    );
  }

  async verifyMemory(partId, memory, file, format = "i") {
    const inputPath = `/tmp/${memory}-verify.${format === "r" ? "bin" : "hex"}`;
    const inputFile = await this.prepareInputFile(inputPath, file);
    return this.run(
      this.builder.verifyMemory(partId, memory, inputPath, format),
      { inputFiles: [inputFile] },
    );
  }

  async eraseChip(partId) {
    return this.run(this.builder.eraseChip(partId));
  }

  async readFuses(partId) {
    const fuseNames = ["lfuse", "hfuse", "efuse", "fuse", "lock"];
    const results = {};
    for (const fuseName of fuseNames) {
      const outputPath = `/tmp/${fuseName}.hex`;
      try {
        const result = await this.run(this.builder.readFuse(partId, fuseName, outputPath));
        results[fuseName] = result.module.FS.readFile(outputPath, { encoding: "utf8" }).trim();
      } catch (error) {
        this.log(`Fuse ${fuseName} not read: ${error.message}`);
      }
    }
    return results;
  }

  async writeFuse(partId, name, value) {
    return this.run(this.builder.writeFuse(partId, name, value));
  }

  async readLockBits(partId) {
    return this.readFuses(partId).then((fuses) => fuses.lock);
  }

  async writeLockBits(partId, value) {
    return this.writeFuse(partId, "lock", value);
  }

  async cancel() {
    throw new Error("Cancel is not exposed until AVRDUDE teardown is proven safe.");
  }

  async run(args, options = {}) {
    if (!this.moduleFactory || !this.configText) {
      throw new Error("AVRDUDE WASM is not initialized.");
    }
    if (!this.connected || !this.selectedPort) {
      throw new Error("Connect a Web Serial port before running AVRDUDE.");
    }
    return this.runRaw(args, options);
  }

  async runRaw(args, { inputFiles = [] } = {}) {
    const command = argsToString(args);
    this.lastOutput = "";
    window.avrdudeLog = [];
    this.log(`$ ${command}`);

    // The upstream build exits its runtime after each startAvrdude() call and
    // keeps global AVRDUDE/getopt state. A new module per operation guarantees
    // that the full command line (-C, -c, -P, -p...) is parsed every time.
    const module = await this.createRuntime(inputFiles);
    const startAvrdude = module.cwrap("startAvrdude", "number", ["string"], { async: true });

    let exitCode = 0;
    let runtimeError = null;

    window.activePort = this.selectedPort;
    window.funcs = module;

    try {
      const result = await Promise.resolve(startAvrdude(command));
      if (Number.isFinite(result)) {
        exitCode = Number(result);
      }
    } catch (error) {
      const emscriptenExitCode = this.getEmscriptenExitCode(error);
      if (emscriptenExitCode === null) {
        runtimeError = error;
      } else {
        exitCode = emscriptenExitCode;
      }
    } finally {
      this.flushAvrdudeLog();

      // avrdude-webassembly clears activePort when its worker closes. Keep the
      // user-selected port for the next fresh AVRDUDE runtime.
      if (this.connected && this.selectedPort) {
        window.activePort = this.selectedPort;
      }
      if (window.funcs === module) {
        window.funcs = null;
      }
    }

    if (runtimeError) {
      throw runtimeError;
    }

    const output = this.lastOutput;
    this.log(`AVRDUDE exit code: ${exitCode}`);
    if (exitCode !== 0) {
      const error = new Error(`AVRDUDE failed with exit code ${exitCode}.`);
      error.exitCode = exitCode;
      error.avrdudeOutput = output;
      throw error;
    }

    return { exitCode, output, command, module };
  }

  async createRuntime(inputFiles = []) {
    const module = await this.moduleFactory({
      locateFile: (path) => this.locateArtifact(path),
      print: (line) => this.captureLine("stdout", line),
      printErr: (line) => this.captureLine("stderr", line),
      noExitRuntime: true,
    });

    module.FS.writeFile("/tmp/avrdude.conf", this.configText);
    for (const inputFile of inputFiles) {
      module.FS.writeFile(inputFile.path, inputFile.data);
    }

    return module;
  }

  getEmscriptenExitCode(error) {
    if (Number.isFinite(error?.status)) {
      return Number(error.status);
    }

    if (error?.name === "ExitStatus") {
      const match = String(error.message ?? "").match(/exit\((-?\d+)\)/i);
      return match ? Number(match[1]) : 0;
    }

    return null;
  }

  flushAvrdudeLog() {
    const lines = Array.isArray(window.avrdudeLog) ? window.avrdudeLog : [];
    for (const line of lines) {
      const text = this.formatDiagnostic(line);
      this.lastOutput += `${text}\n`;
      this.output(`[avrdude] ${text}`);
    }
    window.avrdudeLog = [];
  }

  async prepareInputFile(path, file) {
    if (!file) {
      throw new Error("Select an input file first.");
    }
    const data = new Uint8Array(await file.arrayBuffer());
    this.log(`Loaded ${file.name} (${data.length} byte(s)) for ${path}.`);
    return { path, data };
  }

  captureLine(stream, line) {
    const formatted = this.formatDiagnostic(line);
    const text = `[${stream}] ${formatted}`;
    this.lastOutput += `${formatted}\n`;
    this.output(text);
  }

  formatDiagnostic(value) {
    if (value instanceof Error) {
      return value.message || value.name;
    }
    if (typeof value === "string") {
      return value;
    }
    if (value === null || value === undefined) {
      return String(value);
    }
    try {
      return JSON.stringify(value);
    } catch {
      return String(value);
    }
  }

  parseConfigVersion(configText) {
    const match = configText.match(/^\s*avrdude_conf_version\s*=\s*"([^"]+)"\s*;/m);
    return match?.[1] ?? null;
  }

  locateArtifact(path) {
    if (path.endsWith(".wasm")) {
      return this.artifacts.wasm;
    }
    if (path.endsWith("avrdude-worker.js")) {
      return this.artifacts.worker;
    }
    return new URL(path, this.artifacts.module).href;
  }

  assertRuntimeRequirements() {
    if (!window.isSecureContext) {
      throw new Error("AVR Programmer requires HTTPS or localhost for Web Serial.");
    }
    if (typeof SharedArrayBuffer === "undefined" || !window.crossOriginIsolated) {
      throw new Error(
        "AVRDUDE WASM requires cross-origin isolation. Serve the tool with "
        + "Cross-Origin-Opener-Policy: same-origin and "
        + "Cross-Origin-Embedder-Policy: require-corp (or credentialless).",
      );
    }
  }

  async resolveArtifacts() {
    const required = [
      ["wasm/avrdude.js", LOCAL_ARTIFACTS.module],
      ["wasm/avrdude.wasm", LOCAL_ARTIFACTS.wasm],
      ["wasm/avrdude.conf", LOCAL_ARTIFACTS.conf],
      ["avrdude-worker.js", LOCAL_ARTIFACTS.worker],
    ];
    const checks = await Promise.all(required.map(([, url]) => this.resourceExists(url)));
    const missing = required
      .filter((_, index) => !checks[index])
      .map(([name]) => name);

    if (missing.length) {
      throw new Error(
        `AVRDUDE WASM artifacts are missing: ${missing.join(", ")}. `
        + "Run node scripts/vendor-avrdude-wasm.mjs before deploying.",
      );
    }

    this.log("Using vendored AVRDUDE WebAssembly artifacts.");
    return LOCAL_ARTIFACTS;
  }

  async resourceExists(url) {
    try {
      const response = await fetch(url, { method: "HEAD", cache: "no-store" });
      return response.ok;
    } catch {
      return false;
    }
  }
}
