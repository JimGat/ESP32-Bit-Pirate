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
  programmerProfile: document.querySelector("#programmerProfile"),
  programmerBaudRate: document.querySelector("#programmerBaudRate"),
  busPirateSpiFrequency: document.querySelector("#busPirateSpiFrequency"),
  busPirateSpiLabel: document.querySelector("#busPirateSpiLabel"),
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
  programmingInterface: document.querySelector("#programmingInterface"),
  wasmVersion: document.querySelector("#wasmVersion"),
  progressText: document.querySelector("#progressText"),
  progressBar: document.querySelector("#progressBar"),
  avrdudeOutput: document.querySelector("#avrdudeOutput"),
  technicalLog: document.querySelector("#technicalLog"),
  flashFileName: document.querySelector("#flashFileName"),
  eepromFileName: document.querySelector("#eepromFileName"),
  eraseTarget: document.querySelector("#eraseTarget"),
  actionHint: document.querySelector("#actionHint"),
  modeTabs: [...document.querySelectorAll("[data-mode-tab]")],
  modePanels: [...document.querySelectorAll("[data-mode-panel]")],
  modeActions: [...document.querySelectorAll("[data-mode-actions]")],
};

const PROGRAMMER_PROFILES = {
  buspirate: {
    label: "Bit Pirate / Bus Pirate ISP",
    interfaceLabel: "Bus Pirate Raw SPI / ISP",
    baudRate: 115200,
    busPirateSpi: true,
  },
  arduino: {
    label: "Arduino bootloader",
    interfaceLabel: "Arduino serial bootloader",
    baudRate: 115200,
  },
  stk500v1: {
    label: "STK500v1 / ArduinoISP",
    interfaceLabel: "STK500v1 serial programmer",
    baudRate: 19200,
  },
  stk500v2: {
    label: "STK500v2",
    interfaceLabel: "STK500v2 serial programmer",
    baudRate: 115200,
  },
  avr109: {
    label: "AVR109 bootloader",
    interfaceLabel: "AVR109 serial bootloader",
    baudRate: 57600,
  },
  serialupdi: {
    label: "SerialUPDI",
    interfaceLabel: "SerialUPDI adapter",
    baudRate: 115200,
  },
  jtag2updi: {
    label: "JTAG2UPDI",
    interfaceLabel: "JTAG2UPDI adapter",
    baudRate: 115200,
  },
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
  setActiveMode("target");
  updateProgrammerControls();
  configureAvrdudeProgrammer();

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
  elements.programmerProfile.addEventListener("change", handleProgrammerProfileChange);
  elements.programmerBaudRate.addEventListener("change", configureAvrdudeProgrammer);
  elements.busPirateSpiFrequency.addEventListener("change", configureAvrdudeProgrammer);
  elements.partSearch.addEventListener("change", () => selectPart(repository.findByText(elements.partSearch.value)));
  elements.targetMode.addEventListener("change", () => updateActionHint());
  elements.flashFile.addEventListener("change", () => renderSelectedFile("flash"));
  elements.eepromFile.addEventListener("change", () => renderSelectedFile("eeprom"));
  for (const tab of elements.modeTabs) {
    tab.addEventListener("click", () => setActiveMode(tab.dataset.modeTab));
  }
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
    configureAvrdudeProgrammer();
    const wasmInfo = await engine.initialize();
    elements.wasmVersion.textContent = wasmInfo.version ? `AVRDUDE ${wasmInfo.version}` : "Loaded";

    step("Binding serial bridge", 70);
    await engine.connect(port);
    connected = true;
    setConnectionButtons(true);
    enableOperationButtons(true);
    elements.programmerStatus.textContent = getSelectedProgrammerProfile().label;
    setStatus("Connected");
    updateActionHint();
    log(`${getSelectedProgrammerProfile().label} connected. Auto Detect will identify the target when an operation starts.`);
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
    updateActionHint();
  });
}

async function detect() {
  await run("Reading signature", async ({ step }) => {
    await detectSelectedPart(step);
  });
}

async function detectSelectedPart(step) {
  const selected = requireSelectedPart();
  configureAvrdudeProgrammer();
  step("Initializing programmer", 30);
  const result = await engine.detectPart(selected.id);
  const summary = summarizeOutput(result.output);
  detectedSignature = result.signature ?? summary.signature;
  if (!detectedSignature) {
    detectedPart = null;
    renderPartInfo(selected);
    throw new Error("AVRDUDE completed, but no AVR signature could be read.");
  }

  detectedPart = repository.findBySignature(detectedSignature);
  if (!detectedPart) {
    renderPartInfo(selected, detectedSignature);
    throw new Error(`Detected AVR signature ${detectedSignature}, but no matching part definition was found.`);
  }

  elements.partSearch.value = detectedPart.name;
  renderPartInfo(detectedPart, detectedSignature);
  log(`AVR signature read: ${detectedSignature} (${detectedPart.name}).`);
  return detectedPart;
}

async function readMemory(memory) {
  await run(`Reading ${memory}`, async ({ step }) => {
    configureAvrdudeProgrammer();
    const { part, detectedNow } = await resolvePartForOperation(step);
    if (!detectedNow) {
      await verifySignatureBeforeDestructive(part, false, step);
    }
    const result = await engine.readMemory(part.id, memory, "i");
    const stamp = new Date().toISOString().replace(/[-:]/g, "").slice(0, 15);
    downloadBytes(result.data, `${part.id}-${memory}-${stamp}.hex`);
    log(`${memory} read complete (${result.data.length} byte(s)).`);
  });
}

async function writeMemory(memory, file) {
  await run(`Writing ${memory}`, async ({ step }) => {
    configureAvrdudeProgrammer();
    const { part, detectedNow } = await resolvePartForOperation(step);
    validateFileForMemory(part, memory, file);
    if (!detectedNow) {
      await verifySignatureBeforeDestructive(part, true, step);
    }
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
    configureAvrdudeProgrammer();
    const { part, detectedNow } = await resolvePartForOperation(step);
    validateFileForMemory(part, memory, file);
    if (!detectedNow) {
      await verifySignatureBeforeDestructive(part, false, step);
    }
    await engine.verifyMemory(part.id, memory, file, detectFormat(file));
    log(`${memory} verification complete.`);
  });
}

async function eraseChip() {
  await run("Erasing", async ({ step }) => {
    configureAvrdudeProgrammer();
    const { part, detectedNow } = await resolvePartForOperation(step);
    if (!detectedNow) {
      await verifySignatureBeforeDestructive(part, true, step);
    }
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

async function resolvePartForOperation(step) {
  if (detectedPart) {
    return { part: detectedPart, detectedNow: false };
  }
  if (elements.targetMode.value === "manual") {
    return { part: requireSelectedPart(), detectedNow: false };
  }
  step("Detecting AVR", 12);
  return { part: await detectSelectedPart(step), detectedNow: true };
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
  elements.eraseTarget.textContent = part?.name ?? "Use detected or selected AVR";
  elements.programmingInterface.textContent = getSelectedProgrammerProfile().interfaceLabel;
}

function getSelectedProgrammerProfile() {
  return PROGRAMMER_PROFILES[elements.programmerProfile.value] ?? PROGRAMMER_PROFILES.buspirate;
}

function handleProgrammerProfileChange() {
  const profile = getSelectedProgrammerProfile();
  elements.programmerBaudRate.value = String(profile.baudRate);
  detectedPart = null;
  detectedSignature = null;
  renderPartInfo(repository.findByText(elements.partSearch.value));
  configureAvrdudeProgrammer();
  updateProgrammerControls();
  if (connected) {
    elements.programmerStatus.textContent = profile.label;
    updateActionHint();
  }
}

function configureAvrdudeProgrammer() {
  const profile = getSelectedProgrammerProfile();
  const baudRate = Number.parseInt(elements.programmerBaudRate.value, 10);
  if (!Number.isInteger(baudRate) || baudRate <= 0) {
    throw new Error("Programmer baudrate must be a positive integer.");
  }
  const extendedOptions = profile.busPirateSpi
    ? [`spifreq=${elements.busPirateSpiFrequency.value}`]
    : [];
  engine.configureProgrammer({
    programmer: elements.programmerProfile.value,
    baudRate,
    extendedOptions,
  });
}

function updateProgrammerControls() {
  const isBusPirate = getSelectedProgrammerProfile().busPirateSpi;
  elements.busPirateSpiLabel.hidden = !isBusPirate;
  elements.busPirateSpiFrequency.hidden = !isBusPirate;
}


function setActiveMode(mode) {
  for (const tab of elements.modeTabs) {
    const active = tab.dataset.modeTab === mode;
    tab.classList.toggle("is-active", active);
    tab.setAttribute("aria-selected", String(active));
  }
  for (const panel of elements.modePanels) {
    const active = panel.dataset.modePanel === mode;
    panel.classList.toggle("is-active", active);
    panel.hidden = !active;
  }
  for (const actions of elements.modeActions) {
    const active = actions.dataset.modeActions === mode;
    actions.classList.toggle("is-active", active);
    actions.hidden = !active;
  }
  updateActionHint(mode);
}

function updateActionHint(mode = document.querySelector("[data-mode-tab].is-active")?.dataset.modeTab ?? "target") {
  const automatic = elements.targetMode.value === "auto";
  const hints = {
    flash: "Read a backup, write the selected image, or verify it against the target Flash.",
    eeprom: "Read an EEPROM backup or write the selected EEPROM image.",
    erase: "Chip erase requires a detected target and a final confirmation.",
    target: automatic
      ? "The AVR signature is detected automatically when an operation starts."
      : "Manual selection requires Detect AVR before destructive operations.",
  };
  elements.actionHint.textContent = connected ? hints[mode] : "Connect the programmer to begin.";
}

function renderSelectedFile(memory) {
  const input = memory === "flash" ? elements.flashFile : elements.eepromFile;
  const label = memory === "flash" ? elements.flashFileName : elements.eepromFileName;
  const file = input.files?.[0];
  if (!file) {
    label.textContent = memory === "flash"
      ? "Intel HEX and raw BIN files are supported."
      : "Intel HEX, EEP and raw BIN files are supported.";
    return;
  }
  label.textContent = `${file.name} · ${formatFileSize(file.size)}`;
}

function formatFileSize(bytes) {
  if (bytes >= 1024 * 1024) {
    return `${(bytes / (1024 * 1024)).toFixed(2)} MiB`;
  }
  if (bytes >= 1024) {
    return `${(bytes / 1024).toFixed(1)} KiB`;
  }
  return `${bytes} B`;
}

function renderOperationState({ busy, label, progress }) {
  elements.operationStatus.textContent = label;
  elements.progressText.textContent = label;
  elements.progressBar.style.width = `${Math.max(0, Math.min(100, progress ?? (busy ? 50 : 0)))}%`;
  setControlsBusy(busy);
}

function setControlsBusy(isBusy) {
  for (const control of [
    elements.programmerProfile,
    elements.programmerBaudRate,
    elements.busPirateSpiFrequency,
    elements.targetMode,
    elements.partSearch,
  ]) {
    control.disabled = isBusy;
  }
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
  if (elements.avrdudeOutput.textContent && !elements.avrdudeOutput.textContent.endsWith("\n")) {
    elements.avrdudeOutput.textContent += "\n";
  }
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
