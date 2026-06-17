import { Bpio2Client } from "./Bpio2Client.js";

const elements = {
  serialUnsupported: document.querySelector("#serialUnsupported"),
  connectButton: document.querySelector("#connectButton"),
  disconnectButton: document.querySelector("#disconnectButton"),
  connectionStatus: document.querySelector("#connectionStatus"),
  firmwareVersion: document.querySelector("#firmwareVersion"),
  bpioVersion: document.querySelector("#bpioVersion"),
  currentMode: document.querySelector("#currentMode"),
  spiLimit: document.querySelector("#spiLimit"),
  i2cLimit: document.querySelector("#i2cLimit"),
  pinGrid: document.querySelector("#pinGrid"),
  pinRefreshInterval: document.querySelector("#pinRefreshInterval"),
  pinLiveButton: document.querySelector("#pinLiveButton"),
  operationPanels: document.querySelector(".operation-panels"),
  tabs: [...document.querySelectorAll("[data-operation-tab]")],
  panels: [...document.querySelectorAll("[data-operation-panel]")],
  gpioRows: document.querySelector("#gpioRows"),
  gpioAllHighButton: document.querySelector("#gpioAllHighButton"),
  gpioAllLowButton: document.querySelector("#gpioAllLowButton"),
  gpioInputsButton: document.querySelector("#gpioInputsButton"),
  i2cSpeed: document.querySelector("#i2cSpeed"),
  i2cClockStretch: document.querySelector("#i2cClockStretch"),
  i2cScanButton: document.querySelector("#i2cScanButton"),
  i2cScanProgress: document.querySelector("#i2cScanProgress"),
  i2cDevices: document.querySelector("#i2cDevices"),
  i2cAddress: document.querySelector("#i2cAddress"),
  i2cWrite: document.querySelector("#i2cWrite"),
  i2cRead: document.querySelector("#i2cRead"),
  i2cRunButton: document.querySelector("#i2cRunButton"),
  i2cResult: document.querySelector("#i2cResult"),
  spiSpeed: document.querySelector("#spiSpeed"),
  spiSpeedControls: document.querySelector("#spiSpeedControls"),
  spiCustomSpeed: document.querySelector("#spiCustomSpeed"),
  spiCustomSpeedField: document.querySelector("#spiCustomSpeedField"),
  spiMode: document.querySelector("#spiMode"),
  spiBitOrder: document.querySelector("#spiBitOrder"),
  spiCsIdle: document.querySelector("#spiCsIdle"),
  spiTx: document.querySelector("#spiTx"),
  spiRead: document.querySelector("#spiRead"),
  spiDuplex: document.querySelector("#spiDuplex"),
  spiRunButton: document.querySelector("#spiRunButton"),
  spiResult: document.querySelector("#spiResult"),
  sequenceType: document.querySelector("#sequenceType"),
  sequenceEditors: [...document.querySelectorAll("[data-sequence-editor]")],
  sequenceIo: document.querySelector("#sequenceIo"),
  sequenceLevel: document.querySelector("#sequenceLevel"),
  sequenceDelay: document.querySelector("#sequenceDelay"),
  sequenceDelayUnit: document.querySelector("#sequenceDelayUnit"),
  sequenceSpiTx: document.querySelector("#sequenceSpiTx"),
  sequenceSpiRead: document.querySelector("#sequenceSpiRead"),
  sequenceSpiDuplex: document.querySelector("#sequenceSpiDuplex"),
  sequenceI2cAddress: document.querySelector("#sequenceI2cAddress"),
  sequenceI2cWrite: document.querySelector("#sequenceI2cWrite"),
  sequenceI2cRead: document.querySelector("#sequenceI2cRead"),
  sequenceAddButton: document.querySelector("#sequenceAddButton"),
  sequenceRunButton: document.querySelector("#sequenceRunButton"),
  sequenceClearButton: document.querySelector("#sequenceClearButton"),
  sequenceList: document.querySelector("#sequenceList"),
  sequenceStatus: document.querySelector("#sequenceStatus"),
  clearLogButton: document.querySelector("#clearLogButton"),
  technicalLog: document.querySelector("#technicalLog"),
};

let client = null;
let status = null;
let busy = false;
let sequence = [
  { type: "gpio", io: 4, high: true },
  { type: "delay", value: 100, unit: "ms" },
  { type: "gpio", io: 4, high: false },
];
let livePinMapping = false;
let livePinTimer = null;
let livePinGeneration = 0;
let previousPinValue = null;
const gpioTraceSamples = Array.from({ length: 8 }, () => []);
const gpioRequestedState = new Map();
const modeLimits = { SPI: null, I2C: null };
const MAX_TECHNICAL_LOG_ENTRIES = 512;
const technicalLogEntries = [];

init();

function init() {
  if (!Bpio2Client.isSupported()) {
    elements.serialUnsupported.hidden = false;
    elements.connectButton.disabled = true;
    setConnectionStatus("Unsupported browser");
  }

  elements.connectButton.addEventListener("click", connect);
  elements.disconnectButton.addEventListener("click", disconnect);
  elements.pinLiveButton.addEventListener("click", toggleLivePinMapping);
  elements.pinRefreshInterval.addEventListener("change", handleRefreshIntervalChange);
  elements.tabs.forEach((tab) => tab.addEventListener("click", () => selectTab(tab.dataset.operationTab)));
  elements.gpioAllHighButton.addEventListener("click", () => setAllGpioOutputs(true));
  elements.gpioAllLowButton.addEventListener("click", () => setAllGpioOutputs(false));
  elements.gpioInputsButton.addEventListener("click", setAllGpioInputs);
  elements.i2cScanButton.addEventListener("click", scanI2c);
  elements.i2cRunButton.addEventListener("click", runI2cTransaction);
  elements.spiRunButton.addEventListener("click", runSpiTransfer);
  elements.spiSpeed.addEventListener("change", () => updateSpiCustomSpeedVisibility({ focus: true }));
  elements.sequenceType.addEventListener("change", renderSequenceEditor);
  elements.sequenceDelayUnit.addEventListener("change", updateSequenceDelayLimit);
  elements.sequenceAddButton.addEventListener("click", addSequenceStep);
  elements.sequenceRunButton.addEventListener("click", runSequence);
  elements.sequenceClearButton.addEventListener("click", () => { sequence = []; renderSequence(); });
  elements.sequenceList.addEventListener("click", handleSequenceListClick);
  elements.clearLogButton.addEventListener("click", clearTechnicalLog);

  renderPinGrid();
  renderGpioRows();
  renderSequenceEditor();
  updateSequenceDelayLimit();
  renderSequence();
  updateSpiCustomSpeedVisibility();
  renderControls();
}

async function connect() {
  await runOperation("Connecting", async () => {
    const pending = new Bpio2Client({ log });
    try {
      status = await pending.connect();
      client = pending;
      await probeModeLimits();
    } catch (error) {
      try { await pending.disconnect({ returnToHiZ: false }); } catch { /* Best effort. */ }
      client = null;
      status = null;
      throw error;
    }
    renderStatus();
    log("Connected to BPIO2 adapter.");
  });

  if (client && getActiveTab() === "gpio") {
    startLivePinMapping({ silent: true });
  }
}

async function disconnect() {
  stopLivePinMapping({ silent: true });
  resetGpioTraces();
  await runOperation("Disconnecting", async () => {
    await client?.disconnect();
    client = null;
    status = null;
    gpioRequestedState.clear();
    modeLimits.SPI = null;
    modeLimits.I2C = null;
    setConnectionStatus("Disconnected");
    resetStatus();
  });
}

function toggleLivePinMapping() {
  if (livePinMapping) {
    stopLivePinMapping();
    return;
  }
  startLivePinMapping({ selectGpio: true });
}

function startLivePinMapping({ selectGpio = false, silent = false } = {}) {
  if (!client || !status || busy || livePinMapping) return;

  if (selectGpio) setActiveTab("gpio");
  livePinMapping = true;
  livePinGeneration += 1;
  resetGpioTraces();
  sampleAllGpioTraces(status.ioValue);
  renderLivePinState();
  updateAllLogicTraces();
  renderControls();
  setConnectionStatus("Live pin monitoring");
  if (!silent) log(`Live pin mapping started (${getLivePinInterval()} ms refresh).`);
  pollLivePinMapping(livePinGeneration);
}

function stopLivePinMapping({ silent = false, reason = "" } = {}) {
  const wasActive = livePinMapping;
  livePinMapping = false;
  livePinGeneration += 1;

  if (livePinTimer !== null) {
    window.clearTimeout(livePinTimer);
    livePinTimer = null;
  }

  renderLivePinState();
  renderControls();
  if (client && !busy) setConnectionStatus("Connected");

  if (wasActive && !silent) {
    log(`Live pin mapping stopped${reason ? ` (${reason})` : ""}.`);
  }
}

async function pollLivePinMapping(generation) {
  if (!livePinMapping || generation !== livePinGeneration || !client) return;

  if (busy) {
    scheduleLivePoll(generation);
    return;
  }

  try {
    const nextStatus = await client.getStatus();
    if (!livePinMapping || generation !== livePinGeneration) return;

    // Ignore a sample started before a GPIO/bus command. The command response
    // is newer and remains the source of truth for the requested state.
    if (busy) {
      scheduleLivePoll(generation);
      return;
    }

    // Some firmware builds keep reporting the previous ioDirection bitmap
    // after entering HiZ. Preserve directions explicitly requested by the UI
    // while still using the freshly sampled physical ioValue levels.
    status = applyRequestedGpioState(nextStatus, 0xff);

    sampleAllGpioTraces(status.ioValue);
    renderStatus({ renderGpio: false });
    updateGpioRowsFromStatus();
    updateAllLogicTraces();
  } catch (error) {
    log(`ERROR: live pin mapping stopped: ${error.message}`);
    stopLivePinMapping({ silent: true });
    setConnectionStatus("Error");
    return;
  }

  scheduleLivePoll(generation);
}

function scheduleLivePoll(generation) {
  if (!livePinMapping || generation !== livePinGeneration) return;
  livePinTimer = window.setTimeout(
    () => pollLivePinMapping(generation),
    getLivePinInterval(),
  );
}

function getLivePinInterval() {
  const value = Number(elements.pinRefreshInterval.value);
  return Number.isFinite(value) && value >= 5 && value <= 1000 ? value : 10;
}

function handleRefreshIntervalChange() {
  renderLivePinState();
  updateGpioRowsFromStatus();
  if (!livePinMapping) return;

  livePinGeneration += 1;
  if (livePinTimer !== null) {
    window.clearTimeout(livePinTimer);
    livePinTimer = null;
  }
  pollLivePinMapping(livePinGeneration);
  log(`Live sampling interval changed to ${getLivePinInterval()} ms.`);
}

function renderLivePinState() {
  elements.pinLiveButton.textContent = livePinMapping ? "Stop live" : "Start live";
  elements.pinLiveButton.classList.toggle("is-active", livePinMapping);
}

async function applySingleGpio(io) {
  const row = elements.gpioRows.querySelector(`[data-io-row="${io}"]`);
  if (!row || !client) return;

  const bit = 1 << io;

  const output = row.querySelector("[data-io-direction]").value === "output";
  const high = row.querySelector("[data-io-level]").checked;
  gpioRequestedState.set(io, { output, high });
  updateGpioRowState(io);

  await runOperation(`Updating IO${io}`, async () => {
    const nextStatus = await client.configureGpio({
      directionMask: bit,
      direction: output ? bit : 0,
      valueMask: bit,
      value: high ? bit : 0,
    });
    status = applyRequestedGpioState(nextStatus, bit);
    renderStatus({ renderGpio: false });
    updateGpioRowsFromStatus();
    updateAllLogicTraces();
    log(`GPIO IO${io}: ${output ? "output" : "input"}, ${high ? "HIGH" : "LOW"}.`);
  });
}

async function setAllGpioOutputs(high) {
  await runOperation(`Setting all GPIO ${high ? "HIGH" : "LOW"}`, async () => {
    const allPinsMask = 0xff;
    for (let io = 0; io < 8; io += 1) {
      gpioRequestedState.set(io, { output: true, high });
    }
    const nextStatus = await client.configureGpio({
      directionMask: allPinsMask,
      direction: allPinsMask,
      valueMask: allPinsMask,
      value: high ? allPinsMask : 0,
    });
    status = applyRequestedGpioState(nextStatus, allPinsMask);
    renderStatus({ renderGpio: false });
    updateGpioRowsFromStatus();
    updateAllLogicTraces();
    log(`All GPIOs set ${high ? "HIGH" : "LOW"}.`);
  });
}

async function setAllGpioInputs() {
  await runOperation("Releasing pins", async () => {
    const allPinsMask = 0xff;

    // Keep an explicit INPUT request for every pin. Some firmware builds
    // correctly release the hardware in HiZ but keep the previous direction
    // bitmap in STATUS, which otherwise makes the UI jump back to OUTPUT.
    gpioRequestedState.clear();
    for (let io = 0; io < 8; io += 1) {
      gpioRequestedState.set(io, {
        output: false,
        high: Boolean((status?.ioValue ?? 0) & (1 << io)),
      });
    }

    await client.configureHiZ();
    const nextStatus = await client.configureGpio({
      directionMask: allPinsMask,
      direction: 0,
    });
    status = applyRequestedGpioState(nextStatus, allPinsMask);
    renderStatus();
    updateAllLogicTraces();
    log("All BPIO2 pins explicitly released to HiZ/input mode.");
  });
}

async function scanI2c() {
  await runOperation("Scanning I2C", async () => {
    await configureI2cFromUi();
    elements.i2cDevices.textContent = "Scanning...";
    elements.i2cScanProgress.value = 0;
    elements.i2cScanProgress.hidden = false;
    const found = await client.i2cScan({
      onProgress: ({ progress }) => { elements.i2cScanProgress.value = progress * 100; },
    });
    elements.i2cDevices.innerHTML = "";
    if (!found.length) {
      elements.i2cDevices.textContent = "No devices found.";
    } else {
      for (const address of found) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "device-chip";
        button.textContent = `0x${hexByte(address)}`;
        button.addEventListener("click", () => { elements.i2cAddress.value = `0x${hexByte(address)}`; });
        elements.i2cDevices.append(button);
      }
    }
    log(`I2C scan complete: ${found.map((item) => `0x${hexByte(item)}`).join(", ") || "none"}.`);
  }, () => {
    elements.i2cScanProgress.hidden = true;
  });
}

async function runI2cTransaction() {
  await runOperation("Running I2C", async () => {
    await configureI2cFromUi();
    const address = parseInteger(elements.i2cAddress.value, 0, 0x7f, "I2C address");
    const write = parseHexBytes(elements.i2cWrite.value);
    const readBytes = parseInteger(elements.i2cRead.value, 0, 65535, "I2C read length");
    const result = await client.i2cTransfer({ address, write, readBytes });
    elements.i2cResult.textContent = formatResult(result.data, result.ok ? "Transaction complete." : result.error);
    log(`I2C 0x${hexByte(address)} write=${formatHex(write) || "-"} read=${readBytes}.`);
  });
}

async function runSpiTransfer() {
  await runOperation("Running SPI", async () => {
    await configureSpiFromUi();
    const tx = parseHexBytes(elements.spiTx.value);
    const readBytes = parseInteger(elements.spiRead.value, 0, 65535, "SPI read length");
    const result = await client.spiTransfer({ tx, readBytes, duplex: elements.spiDuplex.checked });
    elements.spiResult.textContent = formatResult(result.data, "Transfer complete.");
    log(`SPI TX=${formatHex(tx) || "-"}, read=${readBytes}, duplex=${elements.spiDuplex.checked}.`);
  });
}



async function configureI2cFromUi() {
  status = await client.configureI2c({
    speed: Number(elements.i2cSpeed.value),
    clockStretch: elements.i2cClockStretch.checked,
  });
  rememberModeLimit("I2C", status);
  gpioRequestedState.clear();
  renderStatus();
}

async function configureSpiFromUi() {
  status = await client.configureSpi({
    speed: getSpiSpeedFromUi(),
    mode: Number(elements.spiMode.value),
    msbFirst: elements.spiBitOrder.value === "msb",
    csIdleHigh: elements.spiCsIdle.value === "high",
  });
  rememberModeLimit("SPI", status);
  gpioRequestedState.clear();
  renderStatus();
}

function updateSpiCustomSpeedVisibility({ focus = false } = {}) {
  const custom = elements.spiSpeed.value === "custom";
  elements.spiCustomSpeedField.hidden = !custom;
  elements.spiSpeedControls.classList.toggle("has-custom", custom);
  if (custom && focus) elements.spiCustomSpeed.focus();
}

function getSpiSpeedFromUi() {
  if (elements.spiSpeed.value !== "custom") {
    return parseInteger(elements.spiSpeed.value, 1, 40_000_000, "SPI speed");
  }
  return parseMegahertz(elements.spiCustomSpeed.value, 40, "SPI speed");
}

function addSequenceStep() {
  try {
    const type = elements.sequenceType.value;
    let step;
    if (type === "gpio") {
      step = { type, io: Number(elements.sequenceIo.value), high: elements.sequenceLevel.value === "high" };
    } else if (type === "delay") {
      const unit = elements.sequenceDelayUnit.value;
      const maximum = unit === "us" ? 100_000 : 60_000;
      step = {
        type,
        value: parseInteger(elements.sequenceDelay.value, 0, maximum, "Delay"),
        unit,
      };
    } else if (type === "spi") {
      step = {
        type,
        tx: [...parseHexBytes(elements.sequenceSpiTx.value)],
        readBytes: parseInteger(elements.sequenceSpiRead.value, 0, 65535, "SPI read length"),
        duplex: elements.sequenceSpiDuplex.checked,
      };
    } else {
      step = {
        type,
        address: parseInteger(elements.sequenceI2cAddress.value, 0, 0x7f, "I2C address"),
        write: [...parseHexBytes(elements.sequenceI2cWrite.value)],
        readBytes: parseInteger(elements.sequenceI2cRead.value, 0, 65535, "I2C read length"),
      };
    }
    sequence.push(step);
    renderSequence();
  } catch (error) {
    setSequenceStatus(error.message, true);
  }
}

async function runSequence() {
  if (!sequence.length) {
    setSequenceStatus("Add at least one step.", true);
    return;
  }

  sequence.forEach((_, index) => markSequenceStep(index, ""));
  await runOperation("Running sequence", async () => {
    setSequenceStatus(`Running 0/${sequence.length}...`);

    for (let index = 0; index < sequence.length; index += 1) {
      const step = sequence[index];
      markSequenceStep(index, "running");

      if (step.type === "gpio") {
        const bit = 1 << step.io;
        gpioRequestedState.set(step.io, { output: true, high: step.high });
        status = await client.configureGpio({
          directionMask: bit,
          direction: bit,
          valueMask: bit,
          value: step.high ? bit : 0,
        });
        status = applyRequestedGpioState(status, bit);
      } else if (step.type === "delay") {
        if (step.unit === "us") {
          delayMicroseconds(step.value);
        } else {
          await delayMilliseconds(step.value);
        }
      } else if (step.type === "spi") {
        await configureSpiFromUi();
        const result = await client.spiTransfer({
          tx: Uint8Array.from(step.tx),
          readBytes: step.readBytes,
          duplex: step.duplex,
        });
        log(`Sequence SPI result: ${formatHex(result.data, 64) || "no data"}.`);
      } else if (step.type === "i2c") {
        await configureI2cFromUi();
        const result = await client.i2cTransfer({
          address: step.address,
          write: Uint8Array.from(step.write),
          readBytes: step.readBytes,
        });
        log(`Sequence I2C 0x${hexByte(step.address)} result: ${formatHex(result.data, 64) || "no data"}.`);
      }

      markSequenceStep(index, "done");
      setSequenceStatus(`Running ${index + 1}/${sequence.length}...`);
    }

    status = applyRequestedGpioState(await client.getStatus(), 0xff);
    renderStatus();
    setSequenceStatus("Sequence complete.");
  });
}

function handleSequenceListClick(event) {
  const button = event.target.closest("[data-remove-step]");
  if (!button) return;
  sequence.splice(Number(button.dataset.removeStep), 1);
  renderSequence();
}

function selectTab(name) {
  if (name !== "gpio") {
    if (livePinMapping) stopLivePinMapping({ reason: `switched to ${name.toUpperCase()}` });
  }

  setActiveTab(name);

  if (name === "gpio" && client && status && !livePinMapping) {
    startLivePinMapping({ silent: true });
  }
}

function setActiveTab(name) {
  elements.tabs.forEach((tab) => {
    const active = tab.dataset.operationTab === name;
    tab.classList.toggle("is-active", active);
    tab.setAttribute("aria-selected", String(active));
  });
  elements.panels.forEach((panel) => {
    const active = panel.dataset.operationPanel === name;
    panel.classList.toggle("is-active", active);
    panel.hidden = !active;
  });
}

function getActiveTab() {
  return elements.tabs.find((tab) => tab.classList.contains("is-active"))?.dataset.operationTab ?? "gpio";
}

function renderStatus({ renderGpio = true } = {}) {
  if (!status) return resetStatus();
  elements.firmwareVersion.textContent = `${status.versionFirmwareMajor}.${status.versionFirmwareMinor}${status.versionFirmwareDate ? ` (${status.versionFirmwareDate})` : ""}`;
  elements.bpioVersion.textContent = `${status.versionFlatbuffersMajor}.${status.versionFlatbuffersMinor}`;
  elements.currentMode.textContent = status.modeCurrent;
  renderTransferLimits();
  renderPinGrid();
  if (renderGpio) renderGpioRows();
  renderControls();
}

function resetStatus() {
  elements.firmwareVersion.textContent = "-";
  elements.bpioVersion.textContent = "-";
  elements.currentMode.textContent = "-";
  elements.spiLimit.textContent = "-";
  elements.i2cLimit.textContent = "-";
  previousPinValue = null;
  renderPinGrid();
  renderGpioRows();
  renderControls();
}

function renderPinGrid() {
  elements.pinGrid.innerHTML = "";
  const labels = getEightPinLabels();
  const leftBank = document.createElement("div");
  const rightBank = document.createElement("div");
  const currentPinValue = status?.ioValue ?? null;

  leftBank.className = "pin-bank pin-bank-left";
  rightBank.className = "pin-bank pin-bank-right";

  labels.forEach((label, index) => {
    const parsed = parsePinLabel(label, index);
    const bit = 1 << index;
    const output = Boolean((status?.ioDirection ?? 0) & bit);
    const high = Boolean((status?.ioValue ?? 0) & bit);
    const roleTags = renderPinRoleTags(index);
    const card = document.createElement("div");
    card.className = [
      "pin-card",
      output ? "is-output" : "is-input",
      high ? "is-high" : "is-low",
      currentPinValue !== null && previousPinValue !== null && Boolean((currentPinValue ^ previousPinValue) & bit) ? "has-changed" : "",
      index < 4 ? "pin-card-left" : "pin-card-right",
    ].filter(Boolean).join(" ");
    card.innerHTML = `
      <span class="pin-connector" aria-hidden="true"></span>
      <div class="pin-card-main">
        <div class="pin-card-title">
          <span class="pin-identity"><span class="pin-index">IO${index}</span>${roleTags}</span>
          <strong>${escapeHtml(parsed.role)}</strong>
        </div>
        <div class="pin-card-meta">
          <span>${escapeHtml(parsed.gpio)}</span>
          <small class="pin-state"><i class="pin-level" aria-hidden="true"></i><span class="pin-state-badge">${status ? (high ? "HIGH" : "LOW") : "OFFLINE"}</span><span>${status ? (output ? "OUT" : "IN") : ""}</span></small>
        </div>
      </div>
    `;
    (index < 4 ? leftBank : rightBank).append(card);
  });

  elements.pinGrid.append(leftBank, rightBank);
  previousPinValue = currentPinValue;
}

function renderGpioRows() {
  elements.gpioRows.innerHTML = "";
  const labels = getEightPinLabels();

  labels.forEach((label, index) => {
    const bit = 1 << index;
    const requested = gpioRequestedState.get(index);
    const observedOutput = Boolean((status?.ioDirection ?? 0) & bit);
    const observedHigh = Boolean((status?.ioValue ?? 0) & bit);
    const directionValue = requested ? (requested.output ? "output" : "input") : (observedOutput ? "output" : "input");
    const high = requested?.output ? requested.high : observedHigh;
    const parsed = parsePinLabel(label, index);
    const row = document.createElement("div");
    row.className = "gpio-row";
    row.dataset.ioRow = String(index);
    const helperText = gpioHelperText({ requested, observedHigh });
    row.innerHTML = `
      <div class="gpio-name">
        <div class="gpio-title-line"><strong>IO${index}</strong>${renderPinRoleTags(index)}</div>
        <span>${escapeHtml(parsed.gpio)}</span>
      </div>
      <select data-io-direction aria-label="IO${index} direction" ${status ? "" : "disabled"}>
        <option value="input" ${directionValue === "input" ? "selected" : ""}>Input</option>
        <option value="output" ${directionValue === "output" ? "selected" : ""}>Output</option>
      </select>
      <label class="level-switch">
        <input type="checkbox" data-io-level ${high ? "checked" : ""} ${status ? "" : "disabled"}>
        <span>${high ? "HIGH" : "LOW"}</span>
      </label>
      <small data-gpio-helper ${helperText ? "" : "hidden"}>${helperText}</small>
      ${renderLogicTrace(index, observedHigh)}
    `;

    const direction = row.querySelector("[data-io-direction]");
    const checkbox = row.querySelector("[data-io-level]");
    direction.addEventListener("change", () => {
      gpioRequestedState.set(index, { output: direction.value === "output", high: checkbox.checked });
      applySingleGpio(index);
    });
    checkbox.addEventListener("change", () => {
      direction.value = "output";
      gpioRequestedState.set(index, { output: true, high: checkbox.checked });
      checkbox.nextElementSibling.textContent = checkbox.checked ? "HIGH" : "LOW";
      applySingleGpio(index);
    });
    elements.gpioRows.append(row);
  });
}

function gpioHelperText({ requested, observedHigh }) {
  if (requested?.output && requested.high !== observedHigh) {
    return `Commanded ${requested.high ? "HIGH" : "LOW"} · observed ${observedHigh ? "HIGH" : "LOW"}`;
  }
  return "";
}

function updateGpioRowState(io) {
  const row = elements.gpioRows.querySelector(`[data-io-row="${io}"]`);
  const requested = gpioRequestedState.get(io);
  if (!row || !requested) return;
  const direction = row.querySelector("[data-io-direction]");
  const checkbox = row.querySelector("[data-io-level]");
  const label = checkbox?.nextElementSibling;
  if (direction) direction.value = requested.output ? "output" : "input";
  if (checkbox) checkbox.checked = requested.high;
  if (label) label.textContent = requested.high ? "HIGH" : "LOW";
}

function applyRequestedGpioState(nextStatus, mask) {
  if (!nextStatus) return nextStatus;
  let direction = nextStatus.ioDirection;
  let value = nextStatus.ioValue;
  for (let io = 0; io < 8; io += 1) {
    const bit = 1 << io;
    if (!(mask & bit)) continue;
    const requested = gpioRequestedState.get(io);
    if (!requested) continue;
    direction = requested.output ? (direction | bit) : (direction & ~bit);
    if (requested.output) value = requested.high ? (value | bit) : (value & ~bit);
  }
  return { ...nextStatus, ioDirection: direction & 0xff, ioValue: value & 0xff };
}

function updateGpioRowsFromStatus() {
  if (!status) return;
  for (let io = 0; io < 8; io += 1) {
    const row = elements.gpioRows.querySelector(`[data-io-row="${io}"]`);
    if (!row) continue;
    const bit = 1 << io;
    const requested = gpioRequestedState.get(io);
    const observedOutput = Boolean(status.ioDirection & bit);
    const observedHigh = Boolean(status.ioValue & bit);
    const displayedOutput = requested ? requested.output : observedOutput;
    const displayedHigh = requested?.output ? requested.high : observedHigh;
    const direction = row.querySelector("[data-io-direction]");
    const checkbox = row.querySelector("[data-io-level]");
    const label = checkbox?.nextElementSibling;
    const helper = row.querySelector("[data-gpio-helper]");

    if (direction && document.activeElement !== direction) direction.value = displayedOutput ? "output" : "input";
    if (checkbox && document.activeElement !== checkbox) checkbox.checked = displayedHigh;
    if (label) label.textContent = displayedHigh ? "HIGH" : "LOW";
    if (helper) {
      const helperText = gpioHelperText({ requested, observedHigh });
      helper.textContent = helperText;
      helper.hidden = !helperText;
    }
  }
}

function rememberModeLimit(mode, modeStatus) {
  if (!modeStatus || !["SPI", "I2C"].includes(mode)) return;
  const write = Number(modeStatus.modeMaxWrite) || 0;
  const read = Number(modeStatus.modeMaxRead) || 0;
  if (write > 0 || read > 0) modeLimits[mode] = { write, read };
  renderTransferLimits();
}

function renderTransferLimits() {
  elements.spiLimit.textContent = formatSpiLimit(modeLimits.SPI);
  elements.i2cLimit.textContent = formatSpiLimit(modeLimits.I2C);
}

async function probeModeLimits() {
  try {
    const i2cStatus = await client.configureI2c({ speed: 100_000, clockStretch: false });
    rememberModeLimit("I2C", i2cStatus);
    const spiStatus = await client.configureSpi({ speed: 1_000_000, mode: 0, msbFirst: true, csIdleHigh: true });
    rememberModeLimit("SPI", spiStatus);
  } finally {
    const allPinsMask = 0xff;
    await client.configureHiZ();
    const nextStatus = await client.configureGpio({
      directionMask: allPinsMask,
      direction: 0,
    });
    gpioRequestedState.clear();
    for (let io = 0; io < 8; io += 1) {
      gpioRequestedState.set(io, {
        output: false,
        high: Boolean((nextStatus?.ioValue ?? 0) & (1 << io)),
      });
    }
    status = applyRequestedGpioState(nextStatus, allPinsMask);
  }
  log("SPI and I2C transfer limits detected; all pins released to input mode.");
}

function sampleAllGpioTraces(ioValue) {
  for (let io = 0; io < 8; io += 1) {
    const samples = gpioTraceSamples[io];
    samples.push(Boolean(ioValue & (1 << io)));
    if (samples.length > 72) samples.shift();
  }
}

function resetGpioTraces() {
  gpioTraceSamples.forEach((samples) => { samples.length = 0; });
}

function renderLogicTrace(io, high) {
  const samples = gpioTraceSamples[io];
  const width = 240;
  const highY = 12;
  const lowY = 42;
  const step = samples.length > 1 ? width / (samples.length - 1) : width;
  let path = "";

  samples.forEach((sample, index) => {
    const x = Number((index * step).toFixed(2));
    const y = sample ? highY : lowY;
    if (index === 0) {
      path = `M ${x} ${y}`;
      return;
    }
    const previousY = samples[index - 1] ? highY : lowY;
    path += ` L ${x} ${previousY} L ${x} ${y}`;
  });

  return `
    <div class="gpio-logic-trace" aria-label="IO${io} logic trace">
      <div><span>Live trace</span><strong>${high ? "HIGH" : "LOW"}</strong></div>
      <svg viewBox="0 0 ${width} 54" preserveAspectRatio="none" role="img" aria-label="Recent HIGH and LOW samples">
        <path class="logic-guide" d="M 0 ${highY} H ${width} M 0 ${lowY} H ${width}"></path>
        <path class="logic-wave" d="${path}"></path>
      </svg>
    </div>
  `;
}

function updateAllLogicTraces() {
  if (!status) return;
  for (let io = 0; io < 8; io += 1) {
    const row = elements.gpioRows.querySelector(`[data-io-row="${io}"]`);
    const trace = row?.querySelector(".gpio-logic-trace");
    if (!trace) continue;
    const high = Boolean(status.ioValue & (1 << io));
    trace.outerHTML = renderLogicTrace(io, high);
  }
}

function renderSequenceEditor() {
  elements.sequenceEditors.forEach((editor) => {
    editor.hidden = editor.dataset.sequenceEditor !== elements.sequenceType.value;
  });
}

function updateSequenceDelayLimit() {
  const microseconds = elements.sequenceDelayUnit.value === "us";
  elements.sequenceDelay.max = microseconds ? "100000" : "60000";
  elements.sequenceDelay.title = microseconds
    ? "Approximate busy-wait delay, up to 100000 µs."
    : "Non-blocking delay, up to 60000 ms.";
}

function renderSequence() {
  elements.sequenceList.innerHTML = "";
  if (!sequence.length) {
    elements.sequenceList.innerHTML = '<p class="empty-state">No steps yet. Add GPIO, delay, SPI or I2C actions.</p>';
  } else {
    sequence.forEach((step, index) => {
      const row = document.createElement("div");
      row.className = "sequence-step";
      row.dataset.sequenceStep = String(index);
      row.innerHTML = `
        <span class="step-number">${index + 1}</span>
        <div><strong>${escapeHtml(sequenceStepTitle(step))}</strong><small>${escapeHtml(sequenceStepDetail(step))}</small></div>
        <span class="step-state" data-step-state></span>
        <button type="button" class="remove-step" data-remove-step="${index}" aria-label="Remove step ${index + 1}">×</button>
      `;
      elements.sequenceList.append(row);
    });
  }
  renderControls();
}

function markSequenceStep(index, state) {
  const row = elements.sequenceList.querySelector(`[data-sequence-step="${index}"]`);
  if (!row) return;
  row.classList.toggle("is-running", state === "running");
  row.classList.toggle("is-done", state === "done");
  row.querySelector("[data-step-state]").textContent = state === "running" ? "Running" : state === "done" ? "Done" : "";
}

function setSequenceStatus(message, error = false) {
  elements.sequenceStatus.textContent = message;
  elements.sequenceStatus.classList.toggle("is-error", error);
}

function renderControls() {
  const connected = Boolean(client && status);
  elements.connectButton.hidden = connected;
  elements.disconnectButton.hidden = !connected;
  elements.disconnectButton.disabled = busy || !connected;
  elements.connectButton.disabled = busy || !Bpio2Client.isSupported();
  elements.pinLiveButton.disabled = busy || !connected;
  elements.pinRefreshInterval.disabled = busy || !connected;

  for (const button of [
    elements.gpioAllHighButton,
    elements.gpioAllLowButton,
    elements.gpioInputsButton,
    elements.i2cScanButton,
    elements.i2cRunButton,
    elements.spiRunButton,
    elements.sequenceRunButton,
  ]) {
    button.disabled = busy || !connected || (button === elements.sequenceRunButton && !sequence.length);
  }
  elements.sequenceAddButton.disabled = busy;
  elements.sequenceClearButton.disabled = busy || !sequence.length;
  for (const control of elements.gpioRows.querySelectorAll("[data-io-direction], [data-io-level]")) {
    control.disabled = busy || !connected;
  }
  elements.operationPanels.inert = false;
  elements.operationPanels.classList.remove("is-live-locked");
  elements.operationPanels.setAttribute("aria-disabled", "false");
  renderLivePinState();
}

async function runOperation(label, operation, cleanup = () => {}) {
  if (busy) return;
  busy = true;
  setConnectionStatus(label);
  renderControls();
  try {
    await operation();
    if (client) {
      setConnectionStatus(livePinMapping ? "Live pin monitoring" : "Connected");
    }
  } catch (error) {
    log(`ERROR: ${error.message}`);
    setConnectionStatus("Error");
    setSequenceStatus(error.message, true);
  } finally {
    cleanup();
    busy = false;
    renderControls();
  }
}

function setConnectionStatus(value) {
  elements.connectionStatus.textContent = value;
}

function log(message) {
  const time = new Date().toLocaleTimeString();
  technicalLogEntries.push(`[${time}] ${message}`);

  if (technicalLogEntries.length > MAX_TECHNICAL_LOG_ENTRIES) {
    technicalLogEntries.splice(0, technicalLogEntries.length - MAX_TECHNICAL_LOG_ENTRIES);
  }

  elements.technicalLog.textContent = `${technicalLogEntries.join("\n")}\n`;
  elements.technicalLog.scrollTop = elements.technicalLog.scrollHeight;
}

function clearTechnicalLog() {
  technicalLogEntries.length = 0;
  elements.technicalLog.textContent = "";
}

function getPinRoleTags(io) {
  return [
    ["SPI CS"],
    ["I2C SCL", "SPI SCK"],
    ["I2C SDA", "SPI MOSI"],
    ["SPI MISO"],
    [],
    [],
    [],
    [],
  ][io] ?? [];
}

function renderPinRoleTags(io) {
  const tags = getPinRoleTags(io);
  if (!tags.length) return "";
  return `<span class="pin-role-tags">${tags.map((tag) => `<span class="pin-role-tag">${escapeHtml(tag)}</span>`).join("")}</span>`;
}

function getEightPinLabels() {
  const labels = [...(status?.modePinLabels ?? [])];
  return Array.from({ length: 8 }, (_, index) => labels[index] ?? `IO${index}`);
}

function parsePinLabel(label, index) {
  const match = String(label).match(/^(.*?)\s+(GPIO\s+\d+)$/i);
  return match ? { role: match[1], gpio: match[2] } : { role: `IO${index}`, gpio: label || "-" };
}

function parseHexBytes(value) {
  const cleaned = String(value).trim();
  if (!cleaned) return new Uint8Array();
  const tokens = cleaned
    .replace(/,/g, " ")
    .split(/\s+/)
    .filter(Boolean);
  const bytes = [];
  for (const token of tokens) {
    const normalized = token.replace(/^0x/i, "");
    if (!/^[0-9a-f]+$/i.test(normalized) || normalized.length % 2 !== 0) {
      if (/^[0-9a-f]$/i.test(normalized)) {
        bytes.push(Number.parseInt(normalized, 16));
        continue;
      }
      throw new Error(`Invalid hex data: ${token}`);
    }
    for (let index = 0; index < normalized.length; index += 2) {
      bytes.push(Number.parseInt(normalized.slice(index, index + 2), 16));
    }
  }
  return Uint8Array.from(bytes);
}

function parseInteger(value, minimum, maximum, label) {
  const text = String(value).trim();
  const number = /^0x/i.test(text) ? Number.parseInt(text.slice(2), 16) : Number.parseInt(text, 10);
  if (!Number.isInteger(number) || number < minimum || number > maximum) {
    throw new Error(`${label} must be between ${minimum} and ${maximum}.`);
  }
  return number;
}

function parseMegahertz(value, maximumMhz, label) {
  const text = String(value).trim();
  if (!/^\d+(?:[.,]\d+)?$/.test(text)) {
    throw new Error(`${label} must be entered as a number in MHz, for example 10 or 22.5.`);
  }

  const megahertz = Number(text.replace(",", "."));
  const frequency = Math.round(megahertz * 1_000_000);

  if (!Number.isFinite(megahertz) || frequency < 1 || megahertz > maximumMhz) {
    throw new Error(`${label} must be greater than 0 and no higher than ${maximumMhz} MHz.`);
  }
  return frequency;
}

function formatResult(data, message) {
  const bytes = data ?? new Uint8Array();
  const preview = bytes.slice(0, 512);
  const suffix = bytes.length > preview.length ? `\nPreview truncated to ${preview.length} bytes.` : "";
  return `${message}\nLength: ${bytes.length}\nHex: ${formatHex(preview) || "-"}\nASCII: ${formatAscii(preview) || "-"}${suffix}`;
}

function formatHex(data, limit = Infinity) {
  const bytes = [...(data ?? [])];
  const preview = bytes.slice(0, limit);
  return preview.map((byte) => hexByte(byte)).join(" ") + (bytes.length > preview.length ? " ..." : "");
}

function formatAscii(data) {
  return [...(data ?? [])].map((byte) => (byte >= 32 && byte <= 126 ? String.fromCharCode(byte) : ".")).join("");
}

function hexByte(value) {
  return Number(value).toString(16).padStart(2, "0").toUpperCase();
}

function formatSpiLimit(limit) {
  if (!limit) return "-";
  return formatCount(limit.write || limit.read);
}


function formatCount(value) {
  if (!value) return "-";
  if (value >= 1024) return `${(value / 1024).toFixed(value % 1024 ? 1 : 0)} KiB`;
  return `${value} B`;
}

function sequenceStepTitle(step) {
  if (step.type === "gpio") return `GPIO IO${step.io} ${step.high ? "HIGH" : "LOW"}`;
  if (step.type === "delay") return `Delay ${step.value} ${step.unit === "us" ? "µs" : "ms"}`;
  if (step.type === "spi") return "SPI transfer";
  return `I2C 0x${hexByte(step.address)}`;
}

function sequenceStepDetail(step) {
  if (step.type === "gpio") return "Sets the pin as an output and changes its level.";
  if (step.type === "delay") {
    return step.unit === "us"
      ? "Approximate browser busy-wait delay."
      : "Non-blocking browser delay.";
  }
  if (step.type === "spi") return `TX ${formatHex(step.tx) || "-"}; read ${step.readBytes}; ${step.duplex ? "full duplex" : "write then read"}.`;
  return `Write ${formatHex(step.write) || "-"}; read ${step.readBytes}.`;
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function delayMilliseconds(ms) {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}

function delayMicroseconds(microseconds) {
  const durationMs = microseconds / 1000;
  const start = performance.now();
  while (performance.now() - start < durationMs) {
    // Intentional busy-wait: short microsecond delays must not yield to the event loop.
  }
}
