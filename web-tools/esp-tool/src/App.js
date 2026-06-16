import { downloadBytes } from "../../shared/files/download.js";
import { EspToolAdapter, flashSizeToBytes } from "./EspToolAdapter.js";

const PRESETS = {
  "esp8266-bin": [
    { address: 0x0, label: "firmware.bin" },
  ],
  "esp32-app": [
    { address: 0x1000, label: "bootloader.bin" },
    { address: 0x8000, label: "partitions.bin" },
    { address: 0x10000, label: "firmware.bin" },
  ],
  "esp32-s2-app": [
    { address: 0x1000, label: "bootloader.bin" },
    { address: 0x8000, label: "partitions.bin" },
    { address: 0x10000, label: "firmware.bin" },
  ],
  "esp32-s3-app": [
    { address: 0x0, label: "bootloader.bin" },
    { address: 0x8000, label: "partitions.bin" },
    { address: 0x10000, label: "firmware.bin" },
  ],
  "esp32-c2-app": [
    { address: 0x0, label: "bootloader.bin" },
    { address: 0x8000, label: "partitions.bin" },
    { address: 0x10000, label: "firmware.bin" },
  ],
  "esp32-c3-app": [
    { address: 0x0, label: "bootloader.bin" },
    { address: 0x8000, label: "partitions.bin" },
    { address: 0x10000, label: "firmware.bin" },
  ],
  "esp32-c6-app": [
    { address: 0x0, label: "bootloader.bin" },
    { address: 0x8000, label: "partitions.bin" },
    { address: 0x10000, label: "firmware.bin" },
  ],
  "esp32-h2-app": [
    { address: 0x0, label: "bootloader.bin" },
    { address: 0x8000, label: "partitions.bin" },
    { address: 0x10000, label: "firmware.bin" },
  ],
  "esp32-p4-app": [
    { address: 0x2000, label: "bootloader.bin" },
    { address: 0x8000, label: "partitions.bin" },
    { address: 0x10000, label: "firmware.bin" },
  ],
  "esp32-c5-app": [
    { address: 0x2000, label: "bootloader.bin" },
    { address: 0x8000, label: "partitions.bin" },
    { address: 0x10000, label: "firmware.bin" },
  ],
  "esp32-factory": [
    { address: 0x1000, label: "bootloader.bin" },
    { address: 0x8000, label: "partitions.bin" },
    { address: 0xe000, label: "boot_app0.bin" },
    { address: 0x10000, label: "factory.bin" },
  ],
  "merged-bin": [
    { address: 0x0, label: "merged-firmware.bin" },
  ],
};

const LAYOUT_GUIDANCE = {
  "esp8266-bin": {
    title: "Drop an ESP8266 firmware image here",
    hint: "single merged or application BIN @ 0x0.",
  },
  "esp32-app": {
    title: "Drop ESP32 parts here",
    hint: "bootloader @ 0x1000, partitions @ 0x8000, firmware @ 0x10000.",
  },
  "esp32-s2-app": {
    title: "Drop ESP32-S2 parts here",
    hint: "bootloader @ 0x1000, partitions @ 0x8000, firmware @ 0x10000.",
  },
  "esp32-s3-app": {
    title: "Drop ESP32-S3 parts here",
    hint: "bootloader @ 0x0, partitions @ 0x8000, firmware @ 0x10000.",
  },
  "esp32-c2-app": {
    title: "Drop ESP32-C2 parts here",
    hint: "bootloader @ 0x0, partitions @ 0x8000, firmware @ 0x10000.",
  },
  "esp32-c3-app": {
    title: "Drop ESP32-C3 parts here",
    hint: "bootloader @ 0x0, partitions @ 0x8000, firmware @ 0x10000.",
  },
  "esp32-c6-app": {
    title: "Drop ESP32-C6 parts here",
    hint: "bootloader @ 0x0, partitions @ 0x8000, firmware @ 0x10000.",
  },
  "esp32-h2-app": {
    title: "Drop ESP32-H2 parts here",
    hint: "bootloader @ 0x0, partitions @ 0x8000, firmware @ 0x10000.",
  },
  "esp32-p4-app": {
    title: "Drop ESP32-P4 parts here",
    hint: "bootloader @ 0x2000, partitions @ 0x8000, firmware @ 0x10000.",
  },
  "esp32-c5-app": {
    title: "Drop ESP32-C5 parts here",
    hint: "bootloader @ 0x2000, partitions @ 0x8000, firmware @ 0x10000.",
  },
  "esp32-factory": {
    title: "Drop ESP32 factory parts here",
    hint: "bootloader @ 0x1000, partitions @ 0x8000, boot_app0 @ 0xE000, factory @ 0x10000.",
  },
};

const DEFAULT_READ_LAYOUT = {
  family: "ESP32",
  bootloaderStart: 0x1000,
  partitionStart: 0x8000,
  partitionSize: 0x1000,
  appStart: 0x10000,
  hasPartitionTable: true,
};

const READ_PRESETS = {
  full: { filename: "esp-full-flash.bin" },
  bootloader: { filename: "esp-bootloader.bin" },
  partitions: { filename: "esp-partitions.bin" },
  app: { filename: "esp-application-area.bin" },
  custom: null,
};

const elements = {
  serialUnsupported: document.querySelector("#serialUnsupported"),
  connectButton: document.querySelector("#connectButton"),
  disconnectButton: document.querySelector("#disconnectButton"),
  baudRate: document.querySelector("#baudRate"),
  connectionStatus: document.querySelector("#connectionStatus"),
  bootModeHint: document.querySelector("#bootModeHint"),
  bootHelp: document.querySelector("#bootHelp"),
  dismissBootHelpButton: document.querySelector("#dismissBootHelpButton"),
  chipModel: document.querySelector("#chipModel"),
  macAddress: document.querySelector("#macAddress"),
  flashId: document.querySelector("#flashId"),
  flashDetectedSize: document.querySelector("#flashDetectedSize"),
  loaderStatus: document.querySelector("#loaderStatus"),
  flashPreset: document.querySelector("#flashPreset"),
  addFlashRowButton: document.querySelector("#addFlashRowButton"),
  flashDropZone: document.querySelector("#flashDropZone"),
  flashFileRows: document.querySelector("#flashFileRows"),
  eraseBeforeFlash: document.querySelector("#eraseBeforeFlash"),
  verifyAfterFlash: document.querySelector("#verifyAfterFlash"),
  rebootAfterFlash: document.querySelector("#rebootAfterFlash"),
  flashSizeSelect: document.querySelector("#flashSizeSelect"),
  readPreset: document.querySelector("#readPreset"),
  readStart: document.querySelector("#readStart"),
  readSize: document.querySelector("#readSize"),
  readFilename: document.querySelector("#readFilename"),
  readPercent: document.querySelector("#readPercent"),
  readSpeed: document.querySelector("#readSpeed"),
  readAmount: document.querySelector("#readAmount"),
  readElapsed: document.querySelector("#readElapsed"),
  addMergeRowButton: document.querySelector("#addMergeRowButton"),
  mergeLayoutPreset: document.querySelector("#mergeLayoutPreset"),
  mergeDropZone: document.querySelector("#mergeDropZone"),
  mergeDropTitle: document.querySelector("#mergeDropTitle"),
  mergeDropHint: document.querySelector("#mergeDropHint"),
  mergeFileRows: document.querySelector("#mergeFileRows"),
  eraseDevice: document.querySelector("#eraseDevice"),
  rebootAfterErase: document.querySelector("#rebootAfterErase"),
  eraseConfirm: document.querySelector("#eraseConfirm"),
  memorySizeLabel: document.querySelector("#memorySizeLabel"),
  memoryStartLabel: document.querySelector("#memoryStartLabel"),
  memoryMiddleLabel: document.querySelector("#memoryMiddleLabel"),
  memoryEndLabel: document.querySelector("#memoryEndLabel"),
  memoryMap: document.querySelector("#memoryMap"),
  validationList: document.querySelector("#validationList"),
  primaryAction: document.querySelector("#primaryAction"),
  operationStatus: document.querySelector("#operationStatus"),
  actionHint: document.querySelector("#actionHint"),
  operationProgress: document.querySelector("#operationProgress"),
  progressValue: document.querySelector("#progressValue"),
  technicalLog: document.querySelector("#technicalLog"),
  copyLogButton: document.querySelector("#copyLogButton"),
  clearLogButton: document.querySelector("#clearLogButton"),
};

const modeTabs = Array.from(document.querySelectorAll("[data-mode-tab]"));
const modePanels = Array.from(document.querySelectorAll("[data-mode-panel]"));
const adapter = new EspToolAdapter({ log, onDeviceLost: handleDeviceLost });

let mode = "flash";
let connected = false;
let busy = false;
let flashRows = [];
let mergeRows = [];
let detectedFlashBytes = null;
let detectedFlashSizeLabel = null;
let detectedChipName = null;
let lastActionHint = "";
let completedMode = null;

init();

function init() {
  if (!("serial" in navigator)) {
    elements.serialUnsupported.hidden = false;
    elements.connectButton.disabled = true;
  }

  bindEvents();
  applyFlashPreset("merged-bin");
  applyMergeLayout("esp32-s3-app");
  updateReadPresetOptions();
  applyReadPreset();
  setStatus("Idle");
  resetProgress();
  render();
  log("Web ESP Tool loaded. Merge works offline; device operations load esptool-js when Connect is pressed.");
}

function bindEvents() {
  elements.connectButton.addEventListener("click", connect);
  elements.disconnectButton.addEventListener("click", disconnect);
  elements.flashPreset.addEventListener("change", () => applyFlashPreset(elements.flashPreset.value));
  elements.addFlashRowButton.addEventListener("click", () => addFlashRow({ address: nextAddress(flashRows) }));
  elements.addMergeRowButton.addEventListener("click", () => addMergeRow({ address: nextAddress(mergeRows) }));
  elements.mergeLayoutPreset.addEventListener("change", () => applyMergeLayout(elements.mergeLayoutPreset.value));
  elements.readPreset.addEventListener("change", applyReadPreset);
  elements.flashSizeSelect.addEventListener("change", syncReadPresetWithDevice);
  elements.primaryAction.addEventListener("click", runPrimaryAction);
  elements.clearLogButton.addEventListener("click", () => {
    elements.technicalLog.textContent = "";
  });
  elements.dismissBootHelpButton.addEventListener("click", () => {
    elements.bootHelp.hidden = true;
  });
  elements.copyLogButton.addEventListener("click", () => navigator.clipboard?.writeText(elements.technicalLog.textContent));

  for (const tab of modeTabs) {
    tab.addEventListener("click", () => {
      completedMode = null;
      selectMode(tab.dataset.modeTab);
    });
  }

  for (const element of [
    elements.eraseBeforeFlash,
    elements.verifyAfterFlash,
    elements.rebootAfterFlash,
    elements.flashSizeSelect,
    elements.readStart,
    elements.readSize,
    elements.readFilename,
    elements.rebootAfterErase,
    elements.eraseConfirm,
  ]) {
    element.addEventListener("input", () => {
      completedMode = null;
      render();
    });
    element.addEventListener("change", () => {
      completedMode = null;
      render();
    });
  }

  wireDropZone(elements.flashDropZone, (files) => addDroppedFiles(files, flashRows));
  wireDropZone(elements.mergeDropZone, (files) => addDroppedFiles(files, mergeRows));
}

async function connect() {
  await runOperation("Connecting", async () => {
    let info;
    try {
      info = await adapter.connect({ baudRate: Number(elements.baudRate.value) });
    } catch (error) {
      error.showBootHelp = shouldShowBootHelp(error);
      throw error;
    }
    connected = true;
    elements.bootHelp.hidden = true;
    detectedFlashSizeLabel = normalizeFlashSizeLabel(info.flashSize);
    detectedFlashBytes = flashSizeToBytes(info.flashSize);
    detectedChipName = info.chipName;
    setConnectionButtons(true);
    renderDeviceInfo(info);
    updateReadPresetOptions();
    syncReadPresetWithDevice();
    log(`Connected to ${info.chipName}.`);
  });
}

async function disconnect() {
  await runOperation("Disconnecting", async () => {
    await adapter.disconnect();
    connected = false;
    detectedFlashBytes = null;
    detectedFlashSizeLabel = null;
    detectedChipName = null;
    setConnectionButtons(false);
    updateReadPresetOptions();
    syncReadPresetWithDevice();
    renderDeviceInfo(null);
  });
}

function handleDeviceLost() {
  connected = false;
  detectedFlashBytes = null;
  detectedFlashSizeLabel = null;
  detectedChipName = null;
  setConnectionButtons(false);
  updateReadPresetOptions();
  syncReadPresetWithDevice();
  renderDeviceInfo(null);
  log("Device disconnected.");
  render();
}

function selectMode(nextMode) {
  mode = nextMode;
  for (const tab of modeTabs) {
    const active = tab.dataset.modeTab === mode;
    tab.classList.toggle("is-active", active);
    tab.setAttribute("aria-selected", String(active));
  }
  for (const panel of modePanels) {
    const active = panel.dataset.modePanel === mode;
    panel.classList.toggle("is-active", active);
    panel.hidden = !active;
  }
  resetProgress();
  render();
}

async function runPrimaryAction() {
  if (mode === "flash") await flashFirmware();
  if (mode === "read") await readFlash();
  if (mode === "merge") await mergeAndDownload();
  if (mode === "erase") await eraseFlash();
}

async function flashFirmware() {
  const files = validRows(flashRows);
  const validation = validateRows(files, getFlashCapacity());
  if (validation.errors.length) {
    throwInline(validation.errors[0]);
    return;
  }

  await runOperation("Flashing", async () => {
    await adapter.writeFlash(files, {
      eraseAll: elements.eraseBeforeFlash.checked,
      verify: elements.verifyAfterFlash.checked,
      reboot: elements.rebootAfterFlash.checked,
      flashSize: getFlashSizeForWrite(),
      onProgress: (fileIndex, written, total) => updateFlashProgress(files, fileIndex, written, total),
    });
    renderProgress(100);
    completedMode = "flash";
    setStatus("Complete");
    log("Flash operation complete.");
  });
}

async function readFlash() {
  await runOperation("Reading", async () => {
    const start = parseNumber(elements.readStart.value);
    const size = parseReadSize(elements.readSize.value);
    const filename = elements.readFilename.value.trim() || "esp-flash-backup.bin";
    if (!Number.isFinite(start) || start < 0) throw new Error("Start address is invalid.");
    if (!Number.isFinite(size) || size <= 0) throw new Error("Read size is invalid.");
    const capacity = getReadFlashCapacity();
    if (start + size > capacity) {
      throw new Error(`Read range exceeds the detected ${formatBytes(capacity)} flash.`);
    }

    const started = performance.now();
    const bytes = await adapter.readFlash({
      start,
      size,
      onProgress: ({ done, total }) => renderReadProgress(done, total, started),
    });
    renderReadProgress(bytes.length, bytes.length, started);
    downloadBytes(bytes, filename);
    log(`Read complete: ${formatBytes(bytes.length)} saved as ${filename}.`);
  });
}

async function mergeAndDownload() {
  try {
    const merged = await buildMergedFirmware();
    downloadBytes(merged.bytes, "merged-firmware.bin");
    renderProgress(100);
    log(`Merged firmware generated: ${formatBytes(merged.bytes.length)}.`);
  } catch (error) {
    showError(error);
  }
}


async function eraseFlash() {
  if (!elements.eraseConfirm.checked) {
    throwInline("Confirm the erase operation first.");
    return;
  }

  const confirmed = window.confirm("Erase the entire ESP flash? This cannot be undone.");
  if (!confirmed) {
    log("Erase cancelled by user.");
    return;
  }

  await runOperation("Erasing", async () => {
    renderProgress(8);
    await adapter.eraseFlash({ reboot: elements.rebootAfterErase.checked });
    renderProgress(100);
    completedMode = "erase";
    setStatus("Complete");
    log("Full flash erase complete.");
  });
}

async function buildMergedFirmware() {
  const files = validRows(mergeRows).sort((a, b) => a.address - b.address);
  const validation = validateRows(files, getFlashCapacity());
  if (!files.length) throw new Error("Add at least one firmware file.");
  if (validation.errors.length) throw new Error(validation.errors[0]);

  const end = Math.max(...files.map((file) => file.address + file.bytes.length));
  const output = new Uint8Array(end);
  output.fill(0xff);
  for (const file of files) {
    output.set(file.bytes, file.address);
  }
  return { bytes: output, files };
}

async function runOperation(label, task) {
  if (busy) return;
  completedMode = null;
  busy = true;
  setButtonsBusy(true);
  setStatus(label);
  render();

  try {
    await task();
    if (!completedMode) {
      setStatus("Idle");
    }
  } catch (error) {
    showError(error);
  } finally {
    busy = false;
    setButtonsBusy(false);
    setConnectionButtons(connected);
    render();
  }
}

function applyFlashPreset(name) {
  if (!PRESETS[name]) return;
  flashRows = PRESETS[name].map((item) => createRow({ address: item.address, label: item.label }));
  elements.flashPreset.value = name;
  render();
}

function applyMergeLayout(name) {
  if (name === "custom") {
    renderMergeGuidance("Custom merge layout", "Add files, set offsets, then merge. Gaps are filled with 0xFF.");
    render();
    return;
  }
  if (!PRESETS[name]) return;
  mergeRows = PRESETS[name].map((item) => createRow({ address: item.address, label: item.label }));
  elements.mergeLayoutPreset.value = name;
  renderMergeGuidanceForLayout(name);
  render();
}

function renderMergeGuidanceForLayout(name) {
  const guidance = LAYOUT_GUIDANCE[name] ?? LAYOUT_GUIDANCE["esp32-app"];
  renderMergeGuidance(guidance.title, guidance.hint);
}

function renderMergeGuidance(title, hint) {
  elements.mergeDropTitle.textContent = title;
  elements.mergeDropHint.textContent = hint;
}

function applyReadPreset() {
  const presetName = elements.readPreset.value;
  const preset = READ_PRESETS[presetName];
  if (!preset) return;

  const layout = getReadLayout();
  const capacity = getReadFlashCapacity();
  let start = 0;
  let size = capacity;

  if (presetName === "bootloader") {
    start = layout.bootloaderStart;
    size = Math.max(0, layout.partitionStart - layout.bootloaderStart);
  } else if (presetName === "partitions") {
    if (!layout.hasPartitionTable) {
      elements.readPreset.value = "custom";
      render();
      return;
    }
    start = layout.partitionStart;
    size = layout.partitionSize;
  } else if (presetName === "app") {
    start = layout.appStart;
    size = Math.max(0, capacity - layout.appStart);
  }

  elements.readStart.value = formatHex(start);
  elements.readSize.value = formatReadSize(size);
  elements.readFilename.value = preset.filename;
  render();
}

function syncReadPresetWithDevice() {
  if (elements.readPreset.value === "custom") return;
  applyReadPreset();
}

function updateReadPresetOptions() {
  const layout = getReadLayout();
  const bootloaderOption = elements.readPreset.querySelector('option[value="bootloader"]');
  const partitionsOption = elements.readPreset.querySelector('option[value="partitions"]');
  const appOption = elements.readPreset.querySelector('option[value="app"]');

  if (bootloaderOption) {
    bootloaderOption.textContent = `Bootloader (${formatHex(layout.bootloaderStart)})`;
  }
  if (partitionsOption) {
    partitionsOption.disabled = !layout.hasPartitionTable;
    partitionsOption.textContent = layout.hasPartitionTable
      ? `Partition Table (${formatHex(layout.partitionStart)})`
      : "Partition Table (not used)";
  }
  if (appOption) {
    appOption.textContent = `Application area (${formatHex(layout.appStart)})`;
  }

  if (!layout.hasPartitionTable && elements.readPreset.value === "partitions") {
    elements.readPreset.value = "full";
  }
}

function getReadLayout() {
  const chip = String(detectedChipName ?? "").toUpperCase();

  if (chip.includes("ESP8266")) {
    return {
      family: "ESP8266",
      bootloaderStart: 0x0,
      partitionStart: 0x1000,
      partitionSize: 0,
      appStart: 0x1000,
      hasPartitionTable: false,
    };
  }

  if (chip.includes("ESP32-C5") || chip.includes("ESP32-P4")) {
    return { ...DEFAULT_READ_LAYOUT, family: chip, bootloaderStart: 0x2000 };
  }

  if (chip.includes("ESP32-S2")) {
    return { ...DEFAULT_READ_LAYOUT, family: chip, bootloaderStart: 0x1000 };
  }

  if (["ESP32-C2", "ESP32-C3", "ESP32-C6", "ESP32-S3", "ESP32-H2"].some((name) => chip.includes(name))) {
    return { ...DEFAULT_READ_LAYOUT, family: chip, bootloaderStart: 0x0 };
  }

  if (chip.includes("ESP32")) {
    return { ...DEFAULT_READ_LAYOUT, family: chip, bootloaderStart: 0x1000 };
  }

  return DEFAULT_READ_LAYOUT;
}


async function addDroppedFiles(files, rows) {
  for (const file of files) {
    const empty = rows.find((row) => !row.file);
    const target = empty ?? createRow({ address: nextAddress(rows) });
    if (!empty) rows.push(target);
    await setRowFile(target, file);
  }
  render();
}

function addFlashRow(initial = {}) {
  flashRows.push(createRow(initial));
  render();
}

function addMergeRow(initial = {}) {
  elements.mergeLayoutPreset.value = "custom";
  renderMergeGuidance("Custom merge layout", "Add files, set offsets, then merge. Gaps are filled with 0xFF.");
  mergeRows.push(createRow(initial));
  render();
}

function render() {
  renderFlashRows();
  renderMergeRows();
  renderMemory();
  renderActions();
}

function renderFlashRows() {
  renderRows(elements.flashFileRows, flashRows, "flash");
}

function renderMergeRows() {
  renderRows(elements.mergeFileRows, mergeRows, "merge");
}

function renderRows(tbody, rows, kind) {
  tbody.textContent = "";
  for (const row of rows) {
    const tr = document.createElement("tr");
    const addressTd = document.createElement("td");
    const addressInput = document.createElement("input");
    addressInput.value = row.addressText;
    addressInput.spellcheck = false;
    addressInput.addEventListener("input", () => {
      row.addressText = addressInput.value;
      row.address = parseNumber(addressInput.value);
      renderMemory();
      renderActions();
    });
    addressTd.append(addressInput);

    const fileTd = document.createElement("td");
    const input = document.createElement("input");
    input.type = "file";
    input.accept = ".bin,application/octet-stream";
    input.addEventListener("change", () => {
      const [file] = input.files;
      if (file) setRowFile(row, file).then(render);
    });
    const label = document.createElement("span");
    label.className = "file-label";
    label.textContent = row.file?.name || (row.label ? `Choose ${row.label}` : "Choose BIN");
    label.addEventListener("click", () => input.click());
    label.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        input.click();
      }
    });
    label.tabIndex = 0;
    fileTd.append(input, label);

    const sizeTd = document.createElement("td");
    sizeTd.textContent = row.bytes ? formatBytes(row.bytes.length) : "-";

    const statusTd = document.createElement("td");
    statusTd.textContent = rowStatus(row);

    const removeTd = document.createElement("td");
    const remove = document.createElement("button");
    remove.type = "button";
    remove.className = "icon-button";
    remove.textContent = "x";
    remove.title = "Remove row";
    remove.addEventListener("click", () => {
      const list = kind === "flash" ? flashRows : mergeRows;
      list.splice(list.indexOf(row), 1);
      render();
    });
    removeTd.append(remove);
    tr.append(addressTd, fileTd, sizeTd, statusTd, removeTd);
    tbody.append(tr);
  }
}

function renderMemory() {
  const rows = validRows(mode === "merge" ? mergeRows : flashRows).sort((a, b) => a.address - b.address);
  const capacity = getFlashCapacity();
  const validation = validateRows(rows, capacity);
  elements.memorySizeLabel.textContent = `${formatBytes(capacity)} view`;
  elements.memoryStartLabel.textContent = formatHex(0);
  elements.memoryMiddleLabel.textContent = formatHex(Math.floor(capacity / 2));
  elements.memoryEndLabel.textContent = formatHex(capacity);
  elements.memoryMap.textContent = "";

  for (const row of rows) {
    const segment = document.createElement("div");
    const hasError = validation.errors.some((error) => error.includes(row.name));
    const startPercent = Math.max(0, Math.min(100, (row.address / capacity) * 100));
    const heightPercent = Math.max(0.6, Math.min(100 - startPercent, (row.bytes.length / capacity) * 100));
    segment.className = hasError ? "memory-segment has-error" : "memory-segment";
    segment.style.top = `${startPercent}%`;
    segment.style.height = `${heightPercent}%`;
    segment.title = `${formatHex(row.address)} ${row.name} ${formatBytes(row.bytes.length)}`;

    const name = document.createElement("strong");
    name.textContent = row.name;
    const details = document.createElement("span");
    details.textContent = `${formatHex(row.address)} · ${formatBytes(row.bytes.length)}`;
    segment.append(name, details);
    elements.memoryMap.append(segment);
  }

  const messages = [...validation.errors, ...validation.warnings];
  if (!rows.length) {
    elements.validationList.textContent = "No files loaded.";
  } else if (!messages.length) {
    elements.validationList.textContent = `${rows.length} file(s), final span ${formatBytes(finalSpan(rows))}.`;
  } else {
    elements.validationList.innerHTML = messages.map((message) => `<div>${escapeHtml(message)}</div>`).join("");
  }
}

function renderActions() {
  elements.primaryAction.classList.toggle("danger", mode === "erase");
  lastActionHint = "";
  if (mode === "flash") {
    elements.primaryAction.textContent = "Flash Firmware";
    const rows = validRows(flashRows);
    const errors = validateRows(rows, getFlashCapacity()).errors;
    const missing = flashRows.some((row) => !row.file);
    elements.primaryAction.disabled = busy || !connected || errors.length > 0 || rows.length === 0 || missing;
    lastActionHint = completedMode === "flash"
      ? ""
      : getFlashHint(rows, errors, missing);
  }
  if (mode === "read") {
    elements.primaryAction.textContent = "Read and Download";
    elements.primaryAction.disabled = busy || !connected;
    lastActionHint = connected ? "Choose a region, then read." : "Connect first.";
  }
  if (mode === "merge") {
    const rows = validRows(mergeRows);
    const errors = validateRows(rows, getFlashCapacity()).errors;
    const missing = mergeRows.some((row) => !row.file);
    elements.primaryAction.textContent = "Merge and Download";
    elements.primaryAction.disabled = busy || errors.length > 0 || rows.length === 0 || missing;
    lastActionHint = getMergeHint(rows, errors, missing);
  }
  if (mode === "erase") {
    elements.primaryAction.textContent = "Erase Entire Flash";
    elements.primaryAction.disabled = busy || !connected || !elements.eraseConfirm.checked;
    lastActionHint = completedMode === "erase"
      ? ""
      : !connected ? "Connect first." : elements.eraseConfirm.checked ? "Ready to erase." : "Tick the erase confirmation.";
  }
  elements.actionHint.textContent = busy ? "Operation running." : lastActionHint;
}

function renderDeviceInfo(info) {
  const chip = info?.chipName ?? "-";
  const mac = info?.macAddress ?? "-";
  const flash = info?.flashSize ?? "-";
  elements.chipModel.textContent = chip;
  elements.macAddress.textContent = mac;
  elements.flashId.textContent = info?.flashId ?? "-";
  elements.flashDetectedSize.textContent = flash;
  elements.loaderStatus.textContent = info ? "esptool-js ready" : "Not loaded";
  elements.eraseDevice.textContent = info ? `${chip} ${mac}` : "Not connected";
}

function updateFlashProgress(files, fileIndex, written, transferTotal) {
  const doneBefore = files.slice(0, fileIndex).reduce((sum, file) => sum + file.bytes.length, 0);
  const currentFileBytes = files[fileIndex]?.bytes.length ?? 0;
  const all = files.reduce((sum, file) => sum + file.bytes.length, 0);
  const currentRatio = transferTotal > 0
    ? Math.max(0, Math.min(1, written / transferTotal))
    : 0;
  const logicalDone = doneBefore + currentFileBytes * currentRatio;
  renderProgress(percent(logicalDone, all));
}

function renderReadProgress(done, total, started) {
  const elapsed = Math.max(0.001, (performance.now() - started) / 1000);
  elements.readPercent.textContent = `${percent(done, total)}%`;
  elements.readSpeed.textContent = `${formatBytes(done / elapsed)}/s`;
  elements.readAmount.textContent = `${formatBytes(done)} / ${formatBytes(total)}`;
  elements.readElapsed.textContent = `${elapsed.toFixed(1)}s`;
  renderProgress(percent(done, total));
}

function renderProgress(value) {
  elements.operationProgress.value = value;
  elements.progressValue.textContent = `${Math.round(value)}%`;
}

function resetProgress() {
  renderProgress(0);
}

async function setRowFile(row, file) {
  row.file = file;
  row.name = file.name;
  row.bytes = new Uint8Array(await file.arrayBuffer());
}

function createRow({ address = 0, label = "" } = {}) {
  return {
    id: crypto.randomUUID?.() ?? String(Date.now() + Math.random()),
    address,
    addressText: formatHex(address),
    label,
    name: "",
    file: null,
    bytes: null,
  };
}

function validRows(rows) {
  return rows.filter((row) => Number.isFinite(row.address) && row.bytes).map((row) => ({
    name: row.name || row.file?.name || row.label || "firmware.bin",
    address: row.address,
    bytes: row.bytes,
  }));
}

function validateRows(rows, capacity) {
  const errors = [];
  const warnings = [];
  const sorted = [...rows].sort((a, b) => a.address - b.address);
  for (let index = 0; index < sorted.length; index += 1) {
    const row = sorted[index];
    if (row.address < 0 || !Number.isFinite(row.address)) errors.push(`${row.name}: invalid address.`);
    if (row.address + row.bytes.length > capacity) errors.push(`${row.name}: exceeds ${formatBytes(capacity)} flash.`);
    const next = sorted[index + 1];
    if (next && row.address + row.bytes.length > next.address) {
      errors.push(`${row.name} overlaps ${next.name}.`);
    }
  }
  for (const row of sorted) {
    if (row.address % 0x1000 !== 0) warnings.push(`${row.name}: address is not 4 KB aligned.`);
  }
  return { errors, warnings };
}

function rowStatus(row) {
  if (!Number.isFinite(row.address)) return "Bad offset";
  if (!row.file) return "Missing file";
  return "Ready";
}

function wireDropZone(element, callback) {
  element.addEventListener("dragover", (event) => {
    event.preventDefault();
    element.classList.add("is-dragging");
  });
  element.addEventListener("dragleave", () => element.classList.remove("is-dragging"));
  element.addEventListener("drop", (event) => {
    event.preventDefault();
    element.classList.remove("is-dragging");
    callback(Array.from(event.dataTransfer.files).filter((file) => /\.bin$/i.test(file.name) || file.type === "application/octet-stream" || !file.type));
  });
}


function parseReadSize(value) {
  if (String(value).trim().toLowerCase() === "detect") {
    return getFlashCapacity();
  }
  return parseNumberWithUnits(value);
}

function parseNumber(value) {
  const text = String(value).trim();
  if (/^0x[0-9a-f]+$/i.test(text)) return Number.parseInt(text, 16);
  return parseNumberWithUnits(text);
}

function parseNumberWithUnits(value) {
  const text = String(value).trim();
  const match = text.match(/^(\d+(?:\.\d+)?)\s*(kb|k|mb|m)?$/i);
  if (!match) return Number.NaN;
  const amount = Number(match[1]);
  const unit = (match[2] ?? "").toLowerCase();
  if (unit === "kb" || unit === "k") return Math.round(amount * 1024);
  if (unit === "mb" || unit === "m") return Math.round(amount * 1024 * 1024);
  return amount;
}

function getFlashCapacity() {
  return detectedFlashBytes || flashSizeToBytes(elements.flashSizeSelect.value) || 4 * 1024 * 1024;
}

function getReadFlashCapacity() {
  return detectedFlashBytes || flashSizeToBytes(elements.flashSizeSelect.value) || 4 * 1024 * 1024;
}

function getFlashSizeForWrite() {
  if (elements.flashSizeSelect.value === "detect" && detectedFlashSizeLabel) {
    return detectedFlashSizeLabel;
  }
  return elements.flashSizeSelect.value;
}

function normalizeFlashSizeLabel(value) {
  const bytes = flashSizeToBytes(value);
  if (!bytes) {
    return null;
  }
  const mb = bytes / 1024 / 1024;
  return Number.isInteger(mb) ? `${mb}MB` : null;
}

function finalSpan(rows) {
  if (!rows.length) return 0;
  return Math.max(...rows.map((row) => row.address + row.bytes.length));
}

function nextAddress(rows) {
  const files = validRows(rows);
  if (!files.length) return 0x0;
  return alignUp(finalSpan(files), 0x1000);
}

function alignUp(value, boundary) {
  return Math.ceil(value / boundary) * boundary;
}

function setConnectionButtons(isConnected) {
  elements.bootModeHint.hidden = isConnected;
  elements.connectButton.hidden = isConnected;
  elements.connectButton.disabled = isConnected;
  elements.disconnectButton.hidden = !isConnected;
  elements.disconnectButton.disabled = !isConnected;
  elements.baudRate.disabled = isConnected;
  elements.connectionStatus.textContent = isConnected ? "Connected" : "Disconnected";
}

function setButtonsBusy(isBusy) {
  for (const button of document.querySelectorAll("button, input, select, textarea")) {
    if (button.id === "clearLogButton" || button.id === "copyLogButton") continue;
    button.disabled = isBusy || button.dataset.disabled === "true";
  }
}

function setStatus(text) {
  elements.operationStatus.textContent = text;
  elements.connectionStatus.textContent = connected ? "Connected" : text === "Idle" ? "Disconnected" : text;
}

function throwInline(message) {
  showError(new Error(message));
}

function showError(error) {
  const message = error instanceof Error ? error.message : String(error);
  setStatus("Error");
  log(`ERROR: ${message}`);
  if (error?.showBootHelp) {
    elements.bootHelp.hidden = false;
    elements.validationList.innerHTML = `<div>Connection failed. Put the ESP in bootloader mode and try again.</div>`;
    return;
  }
  elements.validationList.innerHTML = `<div>${escapeHtml(message)}</div>`;
}

function shouldShowBootHelp(error) {
  const message = error instanceof Error ? error.message : String(error);
  return /connect|sync|timeout|timed out|serial|boot|reset|failed|not responding|no serial data/i.test(message);
}

function getFlashHint(rows, errors, missing) {
  if (!connected) return "Connect first.";
  if (errors.length) return errors[0];
  if (!rows.length || missing) return "Choose each BIN file.";
  if (elements.flashPreset.value === "merged-bin") return "Ready to flash merged BIN at 0x0.";
  return "Ready to flash.";
}

function getMergeHint(rows, errors, missing) {
  if (errors.length) return errors[0];
  if (!rows.length || missing) return "Choose the BIN files for the selected layout, or add custom rows and offsets.";
  return "Ready to merge and download.";
}


function log(message) {
  const text = String(message ?? "").trimEnd();
  if (!text) return;
  const stamp = new Date().toLocaleTimeString();
  elements.technicalLog.textContent += `[${stamp}] ${text}\n`;
  elements.technicalLog.scrollTop = elements.technicalLog.scrollHeight;
}

function percent(done, total) {
  if (!total) return 0;
  return Math.max(0, Math.min(100, Math.round((done / total) * 100)));
}

function formatBytes(bytes) {
  if (!Number.isFinite(bytes)) return "-";
  if (bytes >= 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(bytes >= 10 * 1024 * 1024 ? 1 : 2)} MB`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(bytes >= 10 * 1024 ? 0 : 1)} KB`;
  return `${bytes} B`;
}

function formatReadSize(bytes) {
  if (!Number.isFinite(bytes) || bytes <= 0) return "";
  const mb = bytes / 1024 / 1024;
  if (Number.isInteger(mb)) return `${mb}MB`;
  const kb = bytes / 1024;
  if (Number.isInteger(kb)) return `${kb}KB`;
  return String(bytes);
}

function formatHex(value) {
  return `0x${Math.max(0, value).toString(16).padStart(6, "0")}`;
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (char) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    "\"": "&quot;",
    "'": "&#39;",
  })[char]);
}
