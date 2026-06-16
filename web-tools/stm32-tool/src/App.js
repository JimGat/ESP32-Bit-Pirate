import {
  Stm32Bootloader,
  Stm32BootloaderError,
  commandName,
} from "./Stm32Bootloader.js";
import {
  getEraseUnits,
  getEraseUnitsForRange,
  MEMORY_UNITS,
} from "./Stm32DeviceDatabase.js";
import { looksLikeIntelHex, parseIntelHex } from "./IntelHex.js";
import { downloadBytes } from "../../shared/files/download.js";
import { StlinkWebUsbAdapter, StlinkWebUsbError } from "./StlinkWebUsbAdapter.js";

const elements = Object.fromEntries([
  "serialUnsupported", "connectionTransport", "uartBaudControl", "bootBaudRate", "connectButton", "disconnectButton", "connectionStatus",
  "bootControlMode", "bootModeControl", "bootControlPanel", "bootSignal", "resetSignal", "bootActiveLevel", "resetActiveLevel",
  "manualBootHelp", "stlinkHelp", "consoleTab", "firmwareFileInput", "firmwareDropZone", "firmwareCard",
  "firmwareName", "firmwareFormat", "firmwareSize", "clearFirmwareButton", "flashAddress", "eraseBeforeWrite",
  "verifyAfterWrite", "runAfterWrite", "padBin", "firmwareSegments", "readPreset", "readStart", "readSize",
  "readFilename", "readPercent", "readSpeed", "readAmount", "readElapsed", "eraseRangeFields", "eraseStart",
  "eraseSize", "eraseConfirm", "erasePreview", "refreshInfoButton", "infoDeviceName", "infoDeviceId",
  "infoBootloaderVersion", "infoFlashRange", "commandChips", "manualFlashSize", "manualPageSize",
  "applyMemoryOverrideButton", "consoleBaudRate", "openConsoleButton", "terminal", "deviceName", "deviceFamily",
  "deviceId", "bootloaderVersion", "protectionBytes", "detectedFlashSize", "eraseGeometry", "memorySizeLabel", "memoryMap",
  "infoInterfaceLabel", "interfaceLabel", "logTitle",
  "validationList", "primaryAction", "cancelAction", "operationStatus", "actionHint", "operationProgress",
  "progressValue", "copyLogButton", "clearLogButton", "technicalLog",
].map((id) => [id, document.getElementById(id)]));

const bootloader = new Stm32Bootloader({ log });
const stlink = new StlinkWebUsbAdapter({ log });
const textEncoder = new TextEncoder();
const consoleDecoder = new TextDecoder();

let mode = "flash";
let transportMode = "uart";
let firmware = null;
let busy = false;
let operationController = null;
let memoryOverride = { flashSize: null, pageSize: null };
let terminal = null;
let fitAddon = null;
let lastError = null;

init();

function init() {
  elements.serialUnsupported.hidden = Stm32Bootloader.isSupported() || StlinkWebUsbAdapter.isSupported();
  transportMode = elements.connectionTransport.value;
  initializeTerminal();
  bindEvents();
  wireDropZone(elements.firmwareDropZone, (files) => loadFirmware(files[0]));
  applyReadPreset();
  render();
  log("STM32 Web Tool ready. Select UART Bootloader (Web Serial) or ST-Link (WebUSB).");
}

function bindEvents() {
  elements.connectionTransport.addEventListener("change", changeTransport);
  elements.connectButton.addEventListener("click", connect);
  elements.disconnectButton.addEventListener("click", disconnect);
  elements.bootControlMode.addEventListener("change", () => {
    elements.bootControlPanel.hidden = !isUartTransport() || elements.bootControlMode.value !== "automatic";
    render();
  });
  elements.bootSignal.addEventListener("change", keepBootSignalsDistinct);
  elements.resetSignal.addEventListener("change", keepBootSignalsDistinct);

  document.querySelectorAll("[data-mode-tab]").forEach((button) => {
    button.addEventListener("click", () => selectMode(button.dataset.modeTab));
  });

  elements.firmwareFileInput.addEventListener("change", () => loadFirmware(elements.firmwareFileInput.files?.[0]));
  elements.clearFirmwareButton.addEventListener("click", clearFirmware);
  elements.flashAddress.addEventListener("change", synchronizeBinAddress);
  elements.flashAddress.addEventListener("input", () => {
    if (firmware?.format === "BIN") synchronizeBinAddress(false);
    renderMemory();
    renderActions();
  });

  elements.readPreset.addEventListener("change", applyReadPreset);
  elements.eraseConfirm.addEventListener("change", renderActions);
  document.querySelectorAll('input[name="eraseMode"]').forEach((input) => {
    input.addEventListener("change", () => {
      elements.eraseRangeFields.hidden = getEraseMode() !== "range";
      renderErasePreview();
      renderActions();
    });
  });
  elements.eraseStart.addEventListener("input", renderErasePreview);
  elements.eraseSize.addEventListener("input", renderErasePreview);

  elements.refreshInfoButton.addEventListener("click", refreshDeviceInfo);
  elements.applyMemoryOverrideButton.addEventListener("click", applyMemoryOverride);
  elements.openConsoleButton.addEventListener("click", toggleConsole);
  elements.primaryAction.addEventListener("click", runPrimaryAction);
  elements.cancelAction.addEventListener("click", cancelOperation);

  elements.copyLogButton.addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(elements.technicalLog.textContent);
      setActionHint("Technical log copied.");
    } catch (error) {
      showError(error);
    }
  });
  elements.clearLogButton.addEventListener("click", () => {
    elements.technicalLog.textContent = "";
  });

  window.addEventListener("resize", () => fitAddon?.fit());
  navigator.serial?.addEventListener?.("disconnect", handleSerialDisconnect);
  navigator.usb?.addEventListener?.("disconnect", handleUsbDisconnect);
}

async function changeTransport() {
  if (busy) {
    elements.connectionTransport.value = transportMode;
    return;
  }
  const next = elements.connectionTransport.value;
  if (next === transportMode) return;
  await disconnectActiveTransport({ quiet: true });
  transportMode = next;
  memoryOverride = { flashSize: null, pageSize: null };
  if (!isUartTransport() && mode === "console") selectMode("flash");
  setStatus("Disconnected");
  setActionHint(isUartTransport()
    ? "Enter the STM32 bootloader, then connect."
    : "Connect the ST-Link to the target, then select the probe.");
  applyReadPreset();
  render();
}

async function connect() {
  if (busy) return;
  lastError = null;
  setBusy(true, "Connecting");
  setStatus("Connecting");
  setActionHint(isUartTransport()
    ? "Waiting for the STM32 ROM bootloader..."
    : "Waiting for an ST-Link WebUSB probe...");

  try {
    const signal = createOperationSignal();
    if (isUartTransport()) {
      const options = {
        baudRate: Number(elements.bootBaudRate.value),
        autoBoot: getAutoBootConfig(),
        signal,
      };
      if (bootloader.transport.port) {
        await bootloader.reconnectBootloader(options);
      } else {
        await bootloader.connect({ ...options, requestPort: true });
      }
      bootloader.connected = true;
    } else {
      await stlink.connect({ signal });
    }

    memoryOverride = { flashSize: null, pageSize: null };
    syncMemoryFields();
    applyReadPreset();
    setStatus("Connected");
    const info = getActiveInfo();
    setActionHint(`${info.name ?? "STM32"} identified through ${isUartTransport() ? "UART" : "ST-Link"}.`);
    log(`Connected to ${info.name ?? "STM32"} through ${isUartTransport() ? "UART bootloader" : "ST-Link WebUSB"}.`);
  } catch (error) {
    if (isUartTransport()) bootloader.connected = false;
    else stlink.connected = false;
    setStatus("Connection failed");
    showError(error);
  } finally {
    finishOperationState();
    render();
  }
}

async function disconnect() {
  if (busy) cancelOperation();
  await disconnectActiveTransport({ quiet: false });
  firmware?.format === "BIN" && synchronizeBinAddress(false);
  setStatus("Disconnected");
  setActionHint(isUartTransport()
    ? "Enter the STM32 bootloader, then connect."
    : "Connect the ST-Link to the target, then select the probe.");
  render();
}

async function disconnectActiveTransport({ quiet = false } = {}) {
  try {
    if (bootloader.connected || bootloader.consoleMode || bootloader.transport.port) {
      await bootloader.disconnect({ forgetPort: true });
    }
    if (stlink.connected || stlink.usbDevice) {
      await stlink.disconnect();
    }
  } catch (error) {
    if (!quiet) log(`Disconnect warning: ${error.message}`);
  }
}

function handleSerialDisconnect(event) {
  const lostPort = event.port ?? event.target;
  if (lostPort && lostPort !== bootloader.transport.port) return;
  bootloader.connected = false;
  bootloader.consoleMode = false;
  if (!isUartTransport()) return;
  setStatus("Device removed");
  setActionHint("The serial device was disconnected.");
  log("Serial device disconnected.");
  render();
}

function handleUsbDisconnect(event) {
  if (!stlink.handleUsbDisconnect(event.device ?? event.target)) return;
  if (isUartTransport()) return;
  setStatus("Probe removed");
  setActionHint("The ST-Link probe was disconnected.");
  log("ST-Link probe disconnected.");
  render();
}

function selectMode(nextMode) {
  mode = nextMode;
  document.querySelectorAll("[data-mode-tab]").forEach((button) => {
    const active = button.dataset.modeTab === mode;
    button.classList.toggle("is-active", active);
    button.setAttribute("aria-selected", String(active));
  });
  document.querySelectorAll("[data-mode-panel]").forEach((panel) => {
    const active = panel.dataset.modePanel === mode;
    panel.hidden = !active;
    panel.classList.toggle("is-active", active);
  });
  if (mode === "console") {
    requestAnimationFrame(() => fitAddon?.fit());
  }
  resetProgress();
  renderActions();
}

async function runPrimaryAction() {
  if (mode === "flash") await flashFirmware();
  if (mode === "read") await readFlash();
  if (mode === "erase") await eraseFlash();
  if (mode === "info") await refreshDeviceInfo();
  if (mode === "console") await toggleConsole();
}

async function flashFirmware() {
  const validation = validateFirmware();
  if (validation.errors.length) {
    showInlineError(validation.errors[0]);
    return;
  }

  await runOperation("Flashing", async (signal) => {
    const segments = firmware.segments;
    const totalBytes = segments.reduce((sum, segment) => sum + segment.bytes.length, 0);

    if (!isUartTransport()) {
      const image = mergeFirmwareSegments(segments);
      setActionHint(`Programming ${formatBytes(image.bytes.length)} through ST-Link...`);
      await stlink.flash(image.address, image.bytes, {
        signal,
        progress: ({ done, total }) => renderProgress(scaleProgress(done, total, 5, 95)),
      });
      if (elements.runAfterWrite.checked) {
        setActionHint("Resetting and running the target...");
        await stlink.go(null, { signal });
        setStatus("Firmware running");
      }
      renderProgress(100);
      setActionHint(`Flashed ${formatBytes(totalBytes)} successfully through ST-Link.`);
      log(`ST-Link flash complete: ${formatBytes(totalBytes)} across ${segments.length} source range${segments.length === 1 ? "" : "s"}. Affected sectors were erased and the image was verified by the ST-Link driver.`);
      return;
    }

    const eraseStrategy = elements.eraseBeforeWrite.value;
    if (eraseStrategy === "mass") {
      setActionHint("Erasing the complete flash...");
      await bootloader.massErase({ signal });
      renderProgress(15);
    } else if (eraseStrategy === "needed") {
      const units = collectFirmwareEraseUnits(segments);
      if (!units.length) {
        throw new Stm32BootloaderError("The erase layout is unknown. Choose Entire flash or define a manual page size in Device Info.");
      }
      setActionHint(`Erasing ${units.length} required page${units.length === 1 ? "" : "s"}/sector${units.length === 1 ? "" : "s"}...`);
      await bootloader.erasePages(units.map((unit) => unit.index), {
        signal,
        progress: ({ done, total }) => renderProgress(scaleProgress(done, total, 0, 18)),
      });
    }

    let writtenBefore = 0;
    for (const segment of segments) {
      setActionHint(`Writing ${formatHex(segment.address)}...`);
      await bootloader.writeMemory(segment.address, segment.bytes, {
        signal,
        progress: ({ done }) => {
          const absolute = writtenBefore + done;
          renderProgress(scaleProgress(absolute, totalBytes, eraseStrategy === "none" ? 0 : 18, 78));
        },
      });
      writtenBefore += segment.bytes.length;
    }

    if (elements.verifyAfterWrite.checked) {
      let verifiedBefore = 0;
      for (const segment of segments) {
        setActionHint(`Verifying ${formatHex(segment.address)}...`);
        await bootloader.verifyMemory(segment.address, segment.bytes, {
          signal,
          progress: ({ done }) => {
            const absolute = verifiedBefore + done;
            renderProgress(scaleProgress(absolute, totalBytes, 78, 98));
          },
        });
        verifiedBefore += segment.bytes.length;
      }
      log("Firmware verification passed.");
    }

    if (elements.runAfterWrite.checked) {
      const runAddress = Math.min(...segments.map((segment) => segment.address));
      setActionHint(`Starting firmware at ${formatHex(runAddress)}...`);
      await bootloader.go(runAddress, { signal });
      bootloader.connected = false;
      setStatus("Firmware running");
    }

    renderProgress(100);
    setActionHint(`Flashed ${formatBytes(totalBytes)} successfully.`);
    log(`Flash complete: ${formatBytes(totalBytes)} across ${segments.length} memory range${segments.length === 1 ? "" : "s"}.`);
  });
}

async function readFlash() {
  const start = parseNumber(elements.readStart.value);
  const size = parseSize(elements.readSize.value);
  const filename = elements.readFilename.value.trim() || makeBackupFilename();
  if (!Number.isInteger(start) || start < 0) {
    showInlineError("Read start address is invalid.");
    return;
  }
  if (!Number.isInteger(size) || size <= 0) {
    showInlineError("Read size is invalid.");
    return;
  }
  if (size > 32 * MEMORY_UNITS.MB) {
    showInlineError("Browser backups are limited to 32 MB per operation.");
    return;
  }

  await runOperation("Reading", async (signal) => {
    const started = performance.now();
    const bytes = await getActiveTransport().readMemory(start, size, {
      signal,
      progress: ({ done, total }) => {
        renderReadProgress(done, total, started);
        renderProgress(percent(done, total));
      },
    });
    downloadBytes(bytes, filename);
    renderReadProgress(bytes.length, bytes.length, started);
    renderProgress(100);
    setActionHint(`Saved ${formatBytes(bytes.length)} as ${filename}.`);
    log(`Read complete: ${formatHex(start)} + ${formatBytes(bytes.length)}.`);
  });
}

async function eraseFlash() {
  if (!elements.eraseConfirm.checked) {
    showInlineError("Confirm the destructive erase operation first.");
    return;
  }

  const eraseMode = getEraseMode();
  const confirmation = eraseMode === "mass"
    ? "Erase the entire STM32 flash? This cannot be undone."
    : "Erase every flash page or sector overlapping this range? This cannot be undone.";
  if (!window.confirm(confirmation)) {
    log("Erase cancelled by user.");
    return;
  }

  await runOperation("Erasing", async (signal) => {
    if (eraseMode === "mass") {
      await getActiveTransport().massErase({ signal });
    } else {
      const startAddress = parseNumber(elements.eraseStart.value);
      const size = parseSize(elements.eraseSize.value);
      if (!Number.isInteger(startAddress) || !Number.isInteger(size) || size <= 0) {
        throw new Stm32BootloaderError("Erase range is invalid.");
      }

      if (isUartTransport()) {
        const units = getEraseUnitsForRange(getEffectiveDevice(), getEffectiveFlashSize(), startAddress, size);
        if (!units.length) {
          throw new Stm32BootloaderError("No known flash pages or sectors overlap this range. Define a manual page size or use Entire flash.");
        }
        await bootloader.erasePages(units.map((unit) => unit.index), {
          signal,
          progress: ({ done, total }) => renderProgress(percent(done, total)),
        });
      } else {
        await stlink.eraseRange(startAddress, size, {
          signal,
          progress: ({ done, total }) => renderProgress(percent(done, total)),
        });
      }
    }
    renderProgress(100);
    setActionHint("Erase complete.");
  });
}

async function refreshDeviceInfo() {
  if (!isActiveConnected()) {
    showInlineError(`Connect through ${isUartTransport() ? "the STM32 bootloader" : "ST-Link"} first.`);
    return;
  }
  await runOperation("Refreshing", async (signal) => {
    await getActiveTransport().refreshInfo({ signal });
    syncMemoryFields();
    applyReadPreset();
    renderProgress(100);
    setActionHint("Device information refreshed.");
  });
}

async function toggleConsole() {
  if (busy) return;
  if (!isUartTransport()) {
    showInlineError("The serial console is only available with the UART transport.");
    return;
  }
  if (bootloader.consoleMode) {
    try {
      await bootloader.transport.close({ keepPort: true });
      bootloader.consoleMode = false;
      setStatus("Console closed");
      setActionHint("Re-enter the STM32 bootloader before reconnecting in 8E1 mode.");
      log("Serial console closed.");
    } catch (error) {
      showError(error);
    }
    render();
    return;
  }

  if (!bootloader.transport.port) {
    showInlineError("Select and connect a serial port before opening the console.");
    return;
  }

  setBusy(true, "Opening console");
  try {
    await bootloader.openConsole({
      baudRate: Number(elements.consoleBaudRate.value),
      onData: (bytes) => terminal?.write(consoleDecoder.decode(bytes, { stream: true })),
    });
    setStatus("Console open");
    setActionHint("Type directly in the terminal.");
    terminal?.focus();
    requestAnimationFrame(() => fitAddon?.fit());
  } catch (error) {
    showError(error);
  } finally {
    finishOperationState();
    render();
  }
}

async function runOperation(label, task) {
  if (busy) return;
  if (!isActiveConnected()) {
    showInlineError(`Connect through ${isUartTransport() ? "the STM32 UART bootloader" : "ST-Link WebUSB"} first.`);
    return;
  }

  lastError = null;
  setBusy(true, label);
  resetProgress();
  operationController = new AbortController();
  try {
    await task(operationController.signal);
    setOperationStatus("Complete");
  } catch (error) {
    if (error?.name === "AbortError") {
      setOperationStatus("Cancelled");
      setActionHint("Operation cancelled after the current transaction.");
      log("Operation cancelled.");
    } else {
      setOperationStatus("Error");
      showError(error);
    }
  } finally {
    finishOperationState();
    render();
  }
}

function cancelOperation() {
  operationController?.abort();
  elements.cancelAction.disabled = true;
  setActionHint(`Cancelling after the current ${isUartTransport() ? "bootloader" : "ST-Link"} transaction...`);
}

function createOperationSignal() {
  operationController = new AbortController();
  return operationController.signal;
}

async function loadFirmware(file) {
  if (!file) return;
  try {
    const raw = new Uint8Array(await file.arrayBuffer());
    if (!raw.length) throw new Error("The selected firmware file is empty.");

    const text = new TextDecoder().decode(raw);
    if (looksLikeIntelHex(file.name, text)) {
      const parsed = parseIntelHex(text);
      firmware = {
        file,
        name: file.name,
        format: "Intel HEX",
        size: parsed.totalBytes,
        segments: parsed.segments,
        startAddress: parsed.startAddress,
      };
    } else {
      const address = parseNumber(elements.flashAddress.value);
      if (!Number.isInteger(address)) throw new Error("BIN start address is invalid.");
      firmware = {
        file,
        name: file.name,
        format: "BIN",
        size: raw.length,
        segments: [{ address, bytes: raw }],
        startAddress: address,
      };
    }
    log(`Loaded ${file.name}: ${firmware.format}, ${formatBytes(firmware.size)}.`);
    render();
  } catch (error) {
    showError(error);
  } finally {
    elements.firmwareFileInput.value = "";
  }
}

function clearFirmware() {
  firmware = null;
  render();
}

function synchronizeBinAddress(showErrors = true) {
  if (firmware?.format !== "BIN") return;
  const address = parseNumber(elements.flashAddress.value);
  if (!Number.isInteger(address)) {
    if (showErrors) showInlineError("BIN start address is invalid.");
    return;
  }
  firmware.segments[0].address = address;
  firmware.startAddress = address;
  render();
}

function applyReadPreset() {
  const info = getEffectiveInfo();
  const flashStart = info.flashStart;
  const flashSize = info.flashSize;

  if (elements.readPreset.value === "full") {
    elements.readStart.value = formatHex(flashStart);
    elements.readSize.value = formatSizeInput(flashSize);
  } else if (elements.readPreset.value === "application") {
    const segments = firmware?.segments ?? [];
    if (segments.length) {
      const start = Math.min(...segments.map((segment) => segment.address));
      const end = Math.max(...segments.map((segment) => segment.address + segment.bytes.length));
      elements.readStart.value = formatHex(start);
      elements.readSize.value = formatSizeInput(end - start);
    } else {
      elements.readStart.value = formatHex(flashStart);
      elements.readSize.value = formatSizeInput(flashSize);
    }
  }
  elements.readFilename.value = makeBackupFilename();
}

function applyMemoryOverride() {
  try {
    const flashSizeText = elements.manualFlashSize.value.trim();
    const pageSizeText = elements.manualPageSize.value.trim();
    const flashSize = flashSizeText ? parseSize(flashSizeText) : null;
    const pageSize = pageSizeText ? parseSize(pageSizeText) : null;

    if (flashSizeText && (!Number.isInteger(flashSize) || flashSize <= 0 || flashSize > 32 * MEMORY_UNITS.MB)) {
      throw new Error("Manual flash size is invalid.");
    }
    if (pageSizeText && (!Number.isInteger(pageSize) || pageSize < 256 || pageSize > 1024 * MEMORY_UNITS.KB)) {
      throw new Error("Manual page size is invalid.");
    }

    memoryOverride = { flashSize, pageSize };
    applyReadPreset();
    render();
    log(`Memory override applied: flash ${flashSize ? formatBytes(flashSize) : "detected"}, page ${pageSize ? formatBytes(pageSize) : "database"}.`);
  } catch (error) {
    showError(error);
  }
}

function syncMemoryFields() {
  const info = getActiveInfo();
  elements.manualFlashSize.placeholder = `Detected: ${formatBytes(info.flashSize)}`;
  const geometry = getActiveDevice()?.geometry;
  elements.manualPageSize.placeholder = geometry?.type === "uniform"
    ? `Detected: ${formatBytes(geometry.pageSize)}`
    : "Optional uniform page size";
}

function getAutoBootConfig() {
  if (elements.bootControlMode.value !== "automatic") return { enabled: false };
  return {
    enabled: true,
    bootSignal: elements.bootSignal.value,
    resetSignal: elements.resetSignal.value,
    bootActiveLevel: elements.bootActiveLevel.checked,
    resetActiveLevel: elements.resetActiveLevel.checked,
  };
}

function keepBootSignalsDistinct(event) {
  if (elements.bootSignal.value === elements.resetSignal.value) {
    if (event.target === elements.bootSignal) {
      elements.resetSignal.value = elements.bootSignal.value === "dataTerminalReady" ? "requestToSend" : "dataTerminalReady";
    } else {
      elements.bootSignal.value = elements.resetSignal.value === "dataTerminalReady" ? "requestToSend" : "dataTerminalReady";
    }
  }
}

function isUartTransport() {
  return transportMode === "uart";
}

function getActiveTransport() {
  return isUartTransport() ? bootloader : stlink;
}

function getActiveDevice() {
  return getActiveTransport().device;
}

function getActiveInfo() {
  return getActiveTransport().getDeviceInfo();
}

function isActiveConnected() {
  return isUartTransport() ? bootloader.connected : stlink.connected;
}

function getEffectiveDevice() {
  const base = getActiveDevice() ?? {
    flashStart: MEMORY_UNITS.FLASH_START,
    defaultFlashSize: 128 * MEMORY_UNITS.KB,
    geometry: null,
  };
  if (!memoryOverride.pageSize) return base;
  return { ...base, geometry: { type: "uniform", pageSize: memoryOverride.pageSize } };
}

function getEffectiveFlashSize() {
  const target = getActiveTransport();
  return memoryOverride.flashSize ?? target.flashSize ?? target.device?.defaultFlashSize ?? 128 * MEMORY_UNITS.KB;
}

function getEffectiveInfo() {
  const info = getActiveInfo();
  return {
    ...info,
    flashStart: info.flashStart ?? MEMORY_UNITS.FLASH_START,
    flashSize: getEffectiveFlashSize(),
    geometry: getEffectiveDevice().geometry,
  };
}

function mergeFirmwareSegments(segments) {
  const sorted = [...segments].sort((a, b) => a.address - b.address);
  const start = sorted[0].address;
  const end = Math.max(...sorted.map((segment) => segment.address + segment.bytes.length));
  const span = end - start;
  if (span > 32 * MEMORY_UNITS.MB) {
    throw new StlinkWebUsbError("The combined ST-Link image exceeds the 32 MB browser limit.");
  }
  const bytes = new Uint8Array(span);
  bytes.fill(0xff);
  for (const segment of sorted) bytes.set(segment.bytes, segment.address - start);
  return { address: start, bytes };
}

function collectFirmwareEraseUnits(segments) {
  const units = new Map();
  const device = getEffectiveDevice();
  const flashSize = getEffectiveFlashSize();
  for (const segment of segments) {
    for (const unit of getEraseUnitsForRange(device, flashSize, segment.address, segment.bytes.length)) {
      units.set(unit.index, unit);
    }
  }
  return [...units.values()].sort((a, b) => a.index - b.index);
}

function validateFirmware() {
  const errors = [];
  const warnings = [];
  if (!firmware) {
    errors.push("Select a BIN or Intel HEX firmware file.");
    return { errors, warnings };
  }

  const info = getEffectiveInfo();
  const flashEnd = info.flashStart + info.flashSize;
  for (const segment of firmware.segments) {
    if (segment.address % 4 !== 0) {
      errors.push(`Segment ${formatHex(segment.address)} is not aligned to 4 bytes.`);
    }
    if (segment.bytes.length % 4 !== 0 && !elements.padBin.checked) {
      errors.push(`Segment at ${formatHex(segment.address)} is not a multiple of 4 bytes. Enable 0xFF padding.`);
    }
    if (segment.address < info.flashStart || segment.address + segment.bytes.length > flashEnd) {
      errors.push(`Segment ${formatHex(segment.address)}–${formatHex(segment.address + segment.bytes.length - 1)} is outside ${formatHex(info.flashStart)}–${formatHex(flashEnd - 1)}.`);
    }
  }

  const sorted = [...firmware.segments].sort((a, b) => a.address - b.address);
  for (let index = 1; index < sorted.length; index += 1) {
    if (sorted[index].address < sorted[index - 1].address + sorted[index - 1].bytes.length) {
      errors.push("Firmware memory ranges overlap.");
      break;
    }
  }

  if (isUartTransport() && elements.eraseBeforeWrite.value === "needed" && !getEraseUnits(getEffectiveDevice(), getEffectiveFlashSize()).length) {
    warnings.push("Erase geometry is unknown; choose Entire flash or set a manual page size.");
  }
  if (!isActiveConnected()) errors.push(`Connect through ${isUartTransport() ? "the STM32 bootloader" : "ST-Link"} first.`);
  return { errors, warnings };
}

function render() {
  renderConnection();
  renderFirmware();
  renderDeviceInfo();
  renderMemory();
  renderErasePreview();
  renderActions();
}

function renderConnection() {
  const uart = isUartTransport();
  const portSelected = uart && Boolean(bootloader.transport.port);
  const active = isActiveConnected() || (uart && bootloader.consoleMode);
  const supported = uart ? Stm32Bootloader.isSupported() : StlinkWebUsbAdapter.isSupported();

  elements.connectButton.hidden = active;
  elements.disconnectButton.hidden = !active && !portSelected;
  elements.disconnectButton.disabled = busy || (!active && !portSelected);
  elements.connectButton.disabled = busy || !supported;
  elements.uartBaudControl.hidden = !uart;
  elements.bootModeControl.hidden = !uart;
  elements.bootControlPanel.hidden = !uart || elements.bootControlMode.value !== "automatic";
  elements.manualBootHelp.hidden = !uart || active || elements.bootControlMode.value === "automatic";
  elements.stlinkHelp.hidden = uart || active;
  elements.consoleTab.hidden = !uart;
  elements.connectionTransport.disabled = busy || active;

  if (!uart) {
    elements.eraseBeforeWrite.value = "needed";
    elements.eraseBeforeWrite.disabled = true;
    elements.verifyAfterWrite.checked = true;
    elements.verifyAfterWrite.disabled = true;
    elements.padBin.disabled = true;
  } else {
    elements.eraseBeforeWrite.disabled = busy;
    elements.verifyAfterWrite.disabled = busy;
    elements.padBin.disabled = busy;
  }

  elements.infoInterfaceLabel.textContent = "Interface";
  elements.interfaceLabel.textContent = "Interface";
  elements.logTitle.textContent = uart ? "UART technical log" : "ST-Link technical log";
}

function renderFirmware() {
  const loaded = Boolean(firmware);
  elements.firmwareCard.classList.toggle("empty", !loaded);
  elements.firmwareName.textContent = firmware?.name ?? "No firmware selected";
  elements.firmwareFormat.textContent = firmware?.format ?? "-";
  elements.firmwareSize.textContent = firmware ? formatBytes(firmware.size) : "-";
  elements.clearFirmwareButton.disabled = !loaded || busy;
  elements.flashAddress.disabled = firmware?.format === "Intel HEX" || busy;

  if (!firmware) {
    elements.firmwareSegments.classList.add("empty");
    elements.firmwareSegments.textContent = "No firmware memory ranges loaded.";
    return;
  }

  elements.firmwareSegments.classList.remove("empty");
  elements.firmwareSegments.innerHTML = firmware.segments.map((segment, index) => `
    <div>
      <span>${firmware.segments.length > 1 ? `Range ${index + 1}` : "Memory range"}</span>
      <strong>${formatHex(segment.address)} – ${formatHex(segment.address + segment.bytes.length - 1)}</strong>
      <em>${formatBytes(segment.bytes.length)}</em>
    </div>
  `).join("");
}

function renderDeviceInfo() {
  const info = getEffectiveInfo();
  const connected = Boolean(info.connected);
  const geometry = info.geometry;
  const geometryText = geometry?.type === "uniform"
    ? `${formatBytes(geometry.pageSize)} pages`
    : geometry?.type === "sectors"
      ? `${geometry.sizes.length} sectors`
      : "Unknown";

  elements.deviceName.textContent = connected ? info.name : "-";
  elements.deviceFamily.textContent = connected ? info.family : "-";
  elements.deviceId.textContent = connected ? info.deviceIdText : "-";
  elements.bootloaderVersion.textContent = connected ? info.bootloaderVersionText : "-";
  elements.protectionBytes.textContent = connected ? info.optionBytesText : "-";
  elements.detectedFlashSize.textContent = connected ? formatBytes(info.flashSize) : "-";
  elements.eraseGeometry.textContent = connected ? geometryText : "-";

  elements.infoDeviceName.textContent = connected ? info.name : "Not connected";
  elements.infoDeviceId.textContent = connected ? info.deviceIdText : "-";
  elements.infoBootloaderVersion.textContent = connected ? info.bootloaderVersionText : "-";
  elements.infoFlashRange.textContent = connected
    ? `${formatHex(info.flashStart)} – ${formatHex(info.flashStart + info.flashSize - 1)}`
    : "-";

  if (connected && isUartTransport() && info.commands?.length) {
    elements.commandChips.innerHTML = info.commands.map((command) => `
      <span title="0x${command.toString(16).padStart(2, "0").toUpperCase()}">${escapeHtml(commandName(command).replaceAll("_", " "))}</span>
    `).join("");
  } else if (connected && !isUartTransport()) {
    elements.commandChips.innerHTML = (info.capabilities ?? ["SWD", "READ MEMORY", "FLASH", "ERASE", "RESET / RUN"])
      .map((capability) => `<span>${escapeHtml(String(capability))}</span>`)
      .join("");
  } else {
    elements.commandChips.innerHTML = `<span>Connect through ${isUartTransport() ? "the bootloader" : "ST-Link"} to inspect the target.</span>`;
  }
}

function renderMemory() {
  const info = getEffectiveInfo();
  const flashStart = info.flashStart;
  const flashSize = info.flashSize;
  const flashEnd = flashStart + flashSize;
  elements.memorySizeLabel.textContent = `${formatBytes(flashSize)} view`;

  const units = getEraseUnits(getEffectiveDevice(), flashSize);
  const unitMarks = units.length <= 256
    ? units.map((unit) => {
      const left = ((unit.start - flashStart) / flashSize) * 100;
      return `<i class="memory-boundary" style="left:${clamp(left, 0, 100)}%"></i>`;
    }).join("")
    : "";

  const segmentBlocks = (firmware?.segments ?? []).map((segment, index) => {
    const left = ((segment.address - flashStart) / flashSize) * 100;
    const width = (segment.bytes.length / flashSize) * 100;
    return `<span class="memory-segment" style="left:${clamp(left, 0, 100)}%;width:${Math.max(0.7, clamp(width, 0, 100))}%" title="Range ${index + 1}: ${formatHex(segment.address)} + ${formatBytes(segment.bytes.length)}"></span>`;
  }).join("");

  elements.memoryMap.innerHTML = `
    <div class="memory-addresses"><span>${formatHex(flashStart)}</span><span>${formatHex(flashEnd - 1)}</span></div>
    <div class="memory-track">${unitMarks}${segmentBlocks}</div>
  `;

  const validation = validateFirmware();
  if (!firmware) {
    elements.validationList.textContent = !isActiveConnected()
      ? "Connect a target and load a firmware image."
      : "Load a BIN or Intel HEX image to preview its flash ranges.";
    elements.validationList.className = "validation-list";
  } else if (validation.errors.length) {
    elements.validationList.textContent = validation.errors[0];
    elements.validationList.className = "validation-list error";
  } else if (validation.warnings.length) {
    elements.validationList.textContent = validation.warnings[0];
    elements.validationList.className = "validation-list warning";
  } else {
    elements.validationList.textContent = `${firmware.segments.length} valid range${firmware.segments.length === 1 ? "" : "s"}, ${formatBytes(firmware.size)} total.`;
    elements.validationList.className = "validation-list success";
  }
}

function renderErasePreview() {
  const eraseMode = getEraseMode();
  elements.eraseRangeFields.hidden = eraseMode !== "range";
  if (eraseMode === "mass") {
    elements.erasePreview.textContent = `Entire flash: ${formatHex(getEffectiveInfo().flashStart)} + ${formatBytes(getEffectiveFlashSize())}.`;
    elements.erasePreview.className = "validation-list warning";
    return;
  }

  const start = parseNumber(elements.eraseStart.value);
  const size = parseSize(elements.eraseSize.value);
  const units = getEraseUnitsForRange(getEffectiveDevice(), getEffectiveFlashSize(), start, size);
  if (!Number.isInteger(start) || !Number.isInteger(size) || size <= 0) {
    elements.erasePreview.textContent = "Enter a valid start address and size.";
    elements.erasePreview.className = "validation-list error";
  } else if (!units.length) {
    elements.erasePreview.textContent = "No known pages or sectors overlap this range.";
    elements.erasePreview.className = "validation-list error";
  } else {
    const first = units[0];
    const last = units.at(-1);
    elements.erasePreview.textContent = `${units.length} unit${units.length === 1 ? "" : "s"}: ${first.label} through ${last.label}.`;
    elements.erasePreview.className = "validation-list warning";
  }
}

function renderActions() {
  const validation = validateFirmware();
  const connected = isActiveConnected();
  let label = "Flash Firmware";
  let hint = isUartTransport()
    ? "Enter bootloader mode, then connect."
    : "Connect the ST-Link probe and its SWD target.";
  let disabled = busy;

  if (mode === "flash") {
    label = "Flash Firmware";
    disabled ||= validation.errors.length > 0;
    hint = validation.errors[0] ?? validation.warnings[0] ?? (isUartTransport()
      ? `Ready to flash ${formatBytes(firmware?.size ?? 0)}.`
      : `Ready to flash ${formatBytes(firmware?.size ?? 0)} through ST-Link. Affected sectors will be erased and verified automatically.`);
  } else if (mode === "read") {
    label = "Read and Download";
    disabled ||= !connected;
    hint = connected ? "Choose a preset or enter a custom memory range." : "Connect first.";
  } else if (mode === "erase") {
    label = getEraseMode() === "mass" ? "Erase Entire Flash" : "Erase Selected Range";
    disabled ||= !connected || !elements.eraseConfirm.checked;
    hint = connected ? "Confirm the destructive operation before erasing." : "Connect first.";
  } else if (mode === "info") {
    label = "Refresh Device Info";
    disabled ||= !connected;
    hint = connected
      ? (isUartTransport() ? "Query Device ID, bootloader version and supported commands." : "Refresh the SWD target and ST-Link information.")
      : "Connect first.";
  } else if (mode === "console") {
    label = bootloader.consoleMode ? "Close Console" : "Open Console";
    disabled ||= !isUartTransport() || !bootloader.transport.port;
    hint = bootloader.consoleMode ? "Console is active in 8N1 mode." : "Flash and run the firmware, then open its serial console.";
  }

  elements.primaryAction.textContent = label;
  elements.primaryAction.disabled = disabled;
  elements.primaryAction.classList.toggle("danger", mode === "erase");
  elements.actionHint.textContent = lastError?.message ?? hint;
  elements.openConsoleButton.textContent = bootloader.consoleMode ? "Close Console" : "Open Console";
  elements.openConsoleButton.disabled = busy || !isUartTransport() || !bootloader.transport.port;
  elements.refreshInfoButton.disabled = busy || !connected;
  elements.cancelAction.hidden = !busy;
  elements.cancelAction.disabled = !busy || operationController?.signal.aborted;
}

function initializeTerminal() {
  if (!window.Terminal) {
    elements.terminal.textContent = "xterm.js failed to load.";
    return;
  }
  terminal = new window.Terminal({
    cursorBlink: true,
    convertEol: true,
    fontFamily: 'Menlo, "Courier New", monospace',
    fontSize: 13,
    scrollback: 5000,
    theme: {
      background: "#080a09",
      foreground: "#e0e0e0",
      cursor: "#00ffcc",
      selectionBackground: "#1c4c43",
    },
  });
  const fitCtor = window.FitAddon?.FitAddon || window.FitAddon;
  if (fitCtor) {
    fitAddon = new fitCtor();
    terminal.loadAddon(fitAddon);
  }
  terminal.open(elements.terminal);
  fitAddon?.fit();
  terminal.writeln("\x1b[32mSTM32 serial console\x1b[0m");
  terminal.writeln("Open the console to switch the selected port to 8N1.\r\n");
  terminal.onData(async (data) => {
    if (!bootloader.consoleMode) return;
    try {
      await bootloader.writeConsole(textEncoder.encode(data));
    } catch (error) {
      showError(error);
    }
  });
}

function setBusy(value, status = null) {
  busy = value;
  if (status) setOperationStatus(status);
  document.querySelectorAll("button, input, select, textarea").forEach((control) => {
    if (control === elements.cancelAction) return;
    if (value) {
      control.dataset.wasDisabled = control.disabled ? "1" : "0";
      control.disabled = true;
    } else if (control.dataset.wasDisabled !== undefined) {
      control.disabled = control.dataset.wasDisabled === "1";
      delete control.dataset.wasDisabled;
    }
  });
  renderActions();
}

function finishOperationState() {
  operationController = null;
  setBusy(false);
}

function setStatus(text) {
  elements.connectionStatus.textContent = text;
}

function setOperationStatus(text) {
  elements.operationStatus.textContent = text;
}

function setActionHint(text) {
  lastError = null;
  elements.actionHint.textContent = text;
}

function showInlineError(message) {
  const error = message instanceof Error ? message : new Error(String(message));
  showError(error);
}

function showError(error) {
  const normalized = normalizeError(error);
  lastError = normalized;
  elements.actionHint.textContent = normalized.message;
  setOperationStatus("Error");
  log(`ERROR: ${normalized.message}`);
  renderActions();
}

function normalizeError(error) {
  if (error instanceof Error) return error;
  return new Error(String(error));
}

function renderReadProgress(done, total, started) {
  const elapsedSeconds = Math.max(0.001, (performance.now() - started) / 1000);
  elements.readPercent.textContent = `${Math.round(percent(done, total))}%`;
  elements.readSpeed.textContent = `${formatBytes(done / elapsedSeconds)}/s`;
  elements.readAmount.textContent = `${formatBytes(done)} / ${formatBytes(total)}`;
  elements.readElapsed.textContent = `${elapsedSeconds.toFixed(elapsedSeconds < 10 ? 1 : 0)}s`;
}

function renderProgress(value) {
  const normalized = clamp(Number(value) || 0, 0, 100);
  elements.operationProgress.value = normalized;
  elements.progressValue.textContent = `${Math.round(normalized)}%`;
}

function resetProgress() {
  renderProgress(0);
  elements.readPercent.textContent = "0%";
  elements.readSpeed.textContent = "-";
  elements.readAmount.textContent = "0 B";
  elements.readElapsed.textContent = "0s";
  if (!busy) setOperationStatus("Idle");
}

function getEraseMode() {
  return document.querySelector('input[name="eraseMode"]:checked')?.value ?? "mass";
}

function makeBackupFilename() {
  const device = (getActiveDevice()?.family ?? "stm32").toLowerCase().replace(/[^a-z0-9]+/g, "-");
  const date = new Date().toISOString().slice(0, 10);
  return `${device}-flash-${date}.bin`;
}

function wireDropZone(element, callback) {
  for (const eventName of ["dragenter", "dragover"]) {
    element.addEventListener(eventName, (event) => {
      event.preventDefault();
      element.classList.add("is-dragging");
    });
  }
  for (const eventName of ["dragleave", "drop"]) {
    element.addEventListener(eventName, (event) => {
      event.preventDefault();
      element.classList.remove("is-dragging");
    });
  }
  element.addEventListener("drop", (event) => callback([...event.dataTransfer.files]));
  element.addEventListener("click", () => elements.firmwareFileInput.click());
  element.addEventListener("keydown", (event) => {
    if (event.key === "Enter" || event.key === " ") elements.firmwareFileInput.click();
  });
}

function parseNumber(value) {
  const text = String(value).trim().replaceAll("_", "");
  if (!text) return Number.NaN;
  if (/^0x[0-9a-f]+$/i.test(text)) return Number.parseInt(text.slice(2), 16);
  if (/^[0-9]+$/i.test(text)) return Number.parseInt(text, 10);
  return Number.NaN;
}

function parseSize(value) {
  const text = String(value).trim().replaceAll("_", "");
  const match = text.match(/^((?:0x[0-9a-f]+)|(?:\d+(?:\.\d+)?))\s*(b|kb|kib|mb|mib)?$/i);
  if (!match) return Number.NaN;
  const number = match[1].toLowerCase().startsWith("0x")
    ? Number.parseInt(match[1].slice(2), 16)
    : Number.parseFloat(match[1]);
  const unit = (match[2] ?? "b").toLowerCase();
  const multiplier = unit === "kb" || unit === "kib"
    ? MEMORY_UNITS.KB
    : unit === "mb" || unit === "mib"
      ? MEMORY_UNITS.MB
      : 1;
  return Math.round(number * multiplier);
}

function scaleProgress(done, total, start, end) {
  return start + (end - start) * (total > 0 ? done / total : 0);
}

function percent(done, total) {
  return total > 0 ? (done / total) * 100 : 0;
}

function formatHex(value) {
  return `0x${(Number(value) >>> 0).toString(16).padStart(8, "0").toUpperCase()}`;
}

function formatBytes(value) {
  const bytes = Number(value) || 0;
  if (bytes >= MEMORY_UNITS.MB) return `${(bytes / MEMORY_UNITS.MB).toFixed(bytes % MEMORY_UNITS.MB ? 2 : 0)} MB`;
  if (bytes >= MEMORY_UNITS.KB) return `${(bytes / MEMORY_UNITS.KB).toFixed(bytes % MEMORY_UNITS.KB ? 1 : 0)} KB`;
  return `${Math.round(bytes)} B`;
}

function formatSizeInput(bytes) {
  if (bytes % MEMORY_UNITS.MB === 0) return `${bytes / MEMORY_UNITS.MB}MB`;
  if (bytes % MEMORY_UNITS.KB === 0) return `${bytes / MEMORY_UNITS.KB}KB`;
  return `${bytes}`;
}

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function log(message) {
  const timestamp = new Date().toLocaleTimeString([], { hour12: false });
  elements.technicalLog.textContent += `[${timestamp}] ${message}\n`;
  elements.technicalLog.scrollTop = elements.technicalLog.scrollHeight;
}
