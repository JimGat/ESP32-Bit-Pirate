import { downloadBytes } from "../../shared/files/download.js";
import { AvrdudeEngine } from "./AvrdudeEngine.js";
import { OperationManager } from "./OperationManager.js";
import { TargetRepository, formatSize } from "./TargetRepository.js";
import { summarizeOutput } from "./OutputParser.js";

const elements = {
  serialUnsupported: document.querySelector("#serialUnsupported"),
  coiUnsupported: document.querySelector("#coiUnsupported"),
  connectButton: document.querySelector("#connectButton"),
  disconnectButton: document.querySelector("#disconnectButton"),
  detectButton: document.querySelector("#detectButton"),
  readFlashButton: document.querySelector("#readFlashButton"),
  writeFlashButton: document.querySelector("#writeFlashButton"),
  verifyFlashButton: document.querySelector("#verifyFlashButton"),
  readEepromButton: document.querySelector("#readEepromButton"),
  writeEepromButton: document.querySelector("#writeEepromButton"),
  eraseButton: document.querySelector("#eraseButton"),
  clearLogButton: document.querySelector("#clearLogButton"),
  copyLogButton: document.querySelector("#copyLogButton"),
  downloadLogButton: document.querySelector("#downloadLogButton"),
  connectionStatus: document.querySelector("#connectionStatus"),
  programmerStatus: document.querySelector("#programmerStatus"),
  targetStatus: document.querySelector("#targetStatus"),
  signatureStatus: document.querySelector("#signatureStatus"),
  operationStatus: document.querySelector("#operationStatus"),
  targetMode: document.querySelector("#targetMode"),
  partSearch: document.querySelector("#partSearch"),
  partOptions: document.querySelector("#partOptions"),
  flashFile: document.querySelector("#flashFile"),
  eepromFile: document.querySelector("#eepromFile"),
  partName: document.querySelector("#partName"),
  partId: document.querySelector("#partId"),
  signatureValue: document.querySelector("#signatureValue"),
  flashSize: document.querySelector("#flashSize"),
  eepromSize: document.querySelector("#eepromSize"),
  pageSize: document.querySelector("#pageSize"),
  wasmVersion: document.querySelector("#wasmVersion"),
  progressText: document.querySelector("#progressText"),
  progressBar: document.querySelector("#progressBar"),
  avrdudeOutput: document.querySelector("#avrdudeOutput"),
  technicalLog: document.querySelector("#technicalLog"),
};

const repository = new TargetRepository({ log });
const engine = new AvrdudeEngine({ log, output: appendAvrdudeOutput });
const operationManager = new OperationManager({ onState: renderOperationState });

let connected = false;
let detectedPart = null;
let detectedSignature = null;
const runtimeReady = window.crossOriginIsolated && typeof SharedArrayBuffer !== "undefined";

async function init() {
  if (!("serial" in navigator)) {
    elements.serialUnsupported.hidden = false;
    elements.connectButton.disabled = true;
    setStatus("Unsupported browser");
  }

  bindEvents();
  await repository.load();
  renderPartOptions();
  selectPart(repository.findByText(elements.partSearch.value) ?? repository.all()[0]);

  if (!runtimeReady) {
    elements.connectButton.disabled = true;
    elements.connectButton.dataset.disabled = "true";
    elements.coiUnsupported.hidden = false;
    elements.coiUnsupported.textContent = getIsolationErrorMessage();
    setStatus("Preparing AVRDUDE runtime");
    log(`AVRDUDE runtime unavailable: ${getIsolationErrorMessage()}`);
  }

  log("AVR Programmer loaded. Connect selects the adapter; Detect AVR performs the first hardware transaction.");
}

function bindEvents() {
  elements.connectButton.addEventListener("click", connect);
  elements.disconnectButton.addEventListener("click", disconnect);
  elements.detectButton.addEventListener("click", detect);
  elements.readFlashButton.addEventListener("click", () => readMemory("flash"));
  elements.writeFlashButton.addEventListener("click", () => writeMemory("flash", elements.flashFile.files[0]));
  elements.verifyFlashButton.addEventListener("click", () => verifyMemory("flash", elements.flashFile.files[0]));
  elements.readEepromButton.addEventListener("click", () => readMemory("eeprom"));
  elements.writeEepromButton.addEventListener("click", () => writeMemory("eeprom", elements.eepromFile.files[0]));
  elements.eraseButton.addEventListener("click", eraseChip);
  elements.partSearch.addEventListener("change", () => selectPart(repository.findByText(elements.partSearch.value)));
  elements.clearLogButton.addEventListener("click", () => {
    elements.technicalLog.textContent = "";
    elements.avrdudeOutput.textContent = "";
  });
  elements.copyLogButton.addEventListener("click", () => navigator.clipboard?.writeText(elements.technicalLog.textContent));
  elements.downloadLogButton.addEventListener("click", downloadLog);
}

async function connect() {
  if (!runtimeReady) {
    appendAvrdudeOutput(`ERROR: ${getIsolationErrorMessage()}`);
    return;
  }

  await run("Connecting", async ({ step }) => {
    step("Requesting serial port", 15);
    let port;
    try {
      port = await navigator.serial.requestPort();
    } catch (error) {
      if (isSerialPortSelectionCancelled(error)) {
        throw createCancelledOperation("Serial port selection cancelled.");
      }
      throw error;
    }

    step("Loading AVRDUDE WASM", 45);
    const wasmInfo = await engine.initialize();
    elements.wasmVersion.textContent = wasmInfo.version ? `AVRDUDE ${wasmInfo.version}` : "Loaded";

    step("Binding serial bridge", 70);
    await engine.connect(port);
    connected = true;
    setConnectionButtons(true);
    enableOperationButtons(true);
    elements.programmerStatus.textContent = "ESP32 Bit Pirate AVR";
    setStatus("Connected");
    log("ESP32 Bit Pirate AVR connected. Run Detect AVR for a non-destructive signature read.");
  });
}

async function disconnect() {
  await run("Disconnecting", async () => {
    await engine.disconnect();
    connected = false;
    detectedPart = null;
    detectedSignature = null;
    setConnectionButtons(false);
    enableOperationButtons(false);
    renderPartInfo(repository.findByText(elements.partSearch.value));
    elements.programmerStatus.textContent = "-";
    elements.signatureStatus.textContent = "-";
    setStatus("Disconnected");
  });
}

async function detect() {
  await run("Reading signature", async ({ step }) => {
    const selected = requireSelectedPart();
    step("Initializing programmer", 30);
    const result = await engine.detectPart(selected.id);
    const summary = summarizeOutput(result.output);
    detectedSignature = result.signature ?? summary.signature;
    detectedPart = repository.findBySignature(detectedSignature) ?? selected;
    renderPartInfo(detectedPart, detectedSignature);
    log(detectedSignature
      ? `AVR signature read: ${detectedSignature} (${detectedPart?.name ?? "unknown part"}).`
      : "AVRDUDE completed but no signature was parsed from output.");
  });
}

async function readMemory(memory) {
  await run(`Reading ${memory}`, async ({ step }) => {
    const part = requireDetectedOrManualPart();
    await verifySignatureBeforeDestructive(part, false, step);
    const result = await engine.readMemory(part.id, memory, "i");
    const stamp = new Date().toISOString().replace(/[-:]/g, "").slice(0, 15);
    downloadBytes(result.data, `${part.id}-${memory}-${stamp}.hex`);
    log(`${memory} read complete (${result.data.length} byte(s)).`);
  });
}

async function writeMemory(memory, file) {
  await run(`Writing ${memory}`, async ({ step }) => {
    const part = requireDetectedOrManualPart();
    validateFileForMemory(part, memory, file);
    await verifySignatureBeforeDestructive(part, true, step);
    const confirmed = window.confirm(`Write ${file.name} to ${part.name} ${memory}? A backup is recommended before continuing.`);
    if (!confirmed) {
      throw new Error("Write cancelled by user.");
    }
    await engine.writeMemory(part.id, memory, file, detectFormat(file));
    log(`${memory} write complete. Run verify if AVRDUDE did not verify during the write.`);
  });
}

async function verifyMemory(memory, file) {
  await run(`Verifying ${memory}`, async ({ step }) => {
    const part = requireDetectedOrManualPart();
    validateFileForMemory(part, memory, file);
    await verifySignatureBeforeDestructive(part, false, step);
    await engine.verifyMemory(part.id, memory, file, detectFormat(file));
    log(`${memory} verification complete.`);
  });
}

async function eraseChip() {
  await run("Erasing", async ({ step }) => {
    const part = requireDetectedOrManualPart();
    await verifySignatureBeforeDestructive(part, true, step);
    const confirmed = window.confirm(`Erase ${part.name}? Flash and possibly EEPROM can be erased depending on target configuration.`);
    if (!confirmed) {
      throw new Error("Erase cancelled by user.");
    }
    await engine.eraseChip(part.id);
    log(`Erase complete for ${part.name}.`);
  });
}

async function verifySignatureBeforeDestructive(part, destructive, step) {
  if (elements.targetMode.value === "manual" && !detectedSignature && destructive) {
    throw new Error("Run Detect AVR before destructive manual operations.");
  }
  if (!detectedSignature) {
    return;
  }
  step("Re-reading signature", 22);
  const result = await engine.detectPart(part.id);
  const signature = result.signature ?? summarizeOutput(result.output).signature;
  if (signature && signature !== detectedSignature) {
    throw new Error(`Signature changed from ${detectedSignature} to ${signature}. Operation aborted.`);
  }
}

function requireSelectedPart() {
  const selected = repository.findByText(elements.partSearch.value);
  if (!selected) {
    throw new Error("Select a valid AVR part first.");
  }
  return selected;
}

function requireDetectedOrManualPart() {
  if (detectedPart) {
    return detectedPart;
  }
  if (elements.targetMode.value === "manual") {
    return requireSelectedPart();
  }
  throw new Error("Run Detect AVR first, or switch to Manual Selection.");
}

function validateFileForMemory(part, memory, file) {
  if (!file) {
    throw new Error(`Select a ${memory} file first.`);
  }
  const maxSize = memory === "flash" ? part.flash : part.eeprom;
  if (maxSize && file.size > maxSize * 3 && detectFormat(file) === "i") {
    throw new Error(`${file.name} is unexpectedly large for ${part.name} ${memory}.`);
  }
  if (maxSize && file.size > maxSize && detectFormat(file) === "r") {
    throw new Error(`${file.name} exceeds ${part.name} ${memory} size.`);
  }
}

function detectFormat(file) {
  return /\.bin$/i.test(file?.name ?? "") ? "r" : "i";
}

async function run(label, task) {
  try {
    await operationManager.run(label, task);
  } catch (error) {
    const message = formatDiagnostic(error);
    if (error?.cancelled === true) {
      log(message);
      return;
    }
    appendAvrdudeOutput(`ERROR: ${message}`);
  }
}

function isSerialPortSelectionCancelled(error) {
  return error?.name === "NotFoundError"
    || /no port selected|selection cancelled|user cancelled/i.test(String(error?.message ?? ""));
}

function createCancelledOperation(message) {
  const error = new Error(message);
  error.name = "OperationCancelled";
  error.cancelled = true;
  return error;
}

function getIsolationErrorMessage() {
  if (window.location.protocol === "file:") {
    return "Open this tool through HTTPS or localhost; cross-origin isolation cannot work from a file:// URL.";
  }
  if (!window.isSecureContext) {
    return "Open this tool through HTTPS or localhost so Web Serial and SharedArrayBuffer are available.";
  }
  if (!("serviceWorker" in navigator)) {
    return "Service workers are unavailable. Disable private browsing or use a compatible Chromium browser.";
  }
  return "Cross-origin isolation is being initialized. The page should reload automatically; reload it once manually if it remains on this message.";
}

function formatDiagnostic(value) {
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

function renderPartOptions() {
  elements.partOptions.innerHTML = "";
  for (const part of repository.all()) {
    const option = document.createElement("option");
    option.value = part.name;
    option.label = `${part.name} (${part.id})`;
    elements.partOptions.append(option);
  }
}

function selectPart(part) {
  if (!part) {
    return;
  }
  elements.partSearch.value = part.name;
  renderPartInfo(part, detectedSignature);
}

function renderPartInfo(part, signature = null) {
  elements.partName.textContent = part?.name ?? "-";
  elements.partId.textContent = part?.id ?? "-";
  elements.signatureValue.textContent = signature ?? "-";
  elements.flashSize.textContent = formatSize(part?.flash);
  elements.eepromSize.textContent = formatSize(part?.eeprom);
  elements.pageSize.textContent = part?.page ? `${part.page} bytes` : "-";
  elements.targetStatus.textContent = part?.name ?? "-";
  elements.signatureStatus.textContent = signature ?? "-";
}

function renderOperationState({ busy, label, progress }) {
  elements.operationStatus.textContent = label;
  elements.progressText.textContent = label;
  elements.progressBar.style.width = `${Math.max(0, Math.min(100, progress ?? (busy ? 50 : 0)))}%`;
  setControlsBusy(busy);
}

function setControlsBusy(isBusy) {
  for (const button of document.querySelectorAll("button")) {
    if (button === elements.disconnectButton && connected) {
      button.disabled = isBusy;
    } else if (button !== elements.connectButton || !connected) {
      button.disabled = isBusy || button.dataset.disabled === "true";
    }
  }
  if (!isBusy) {
    setConnectionButtons(connected);
    enableOperationButtons(connected);
  }
}

function enableOperationButtons(enabled) {
  for (const button of [
    elements.detectButton,
    elements.readFlashButton,
    elements.writeFlashButton,
    elements.verifyFlashButton,
    elements.readEepromButton,
    elements.writeEepromButton,
    elements.eraseButton,
  ]) {
    button.disabled = !enabled;
    button.dataset.disabled = enabled ? "false" : "true";
  }
}

function setConnectionButtons(isConnected) {
  elements.connectButton.hidden = isConnected;
  elements.disconnectButton.hidden = !isConnected;
  elements.connectButton.disabled = isConnected;
  elements.disconnectButton.disabled = !isConnected;
}

function setStatus(value) {
  elements.connectionStatus.textContent = value;
}

function appendAvrdudeOutput(message) {
  elements.avrdudeOutput.textContent += `${message}\n`;
  elements.avrdudeOutput.scrollTop = elements.avrdudeOutput.scrollHeight;
  log(message);
}

function log(message) {
  const line = `[${new Date().toLocaleTimeString()}] ${message}`;
  elements.technicalLog.textContent += `${line}\n`;
  elements.technicalLog.scrollTop = elements.technicalLog.scrollHeight;
}

function downloadLog() {
  const bytes = new TextEncoder().encode(elements.technicalLog.textContent);
  const stamp = new Date().toISOString().replace(/[-:]/g, "").slice(0, 15);
  downloadBytes(bytes, `avr-programmer-log-${stamp}.txt`);
}

init();
