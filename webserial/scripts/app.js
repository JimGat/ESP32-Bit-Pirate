import { SerialTerminal } from "./serial-terminal.js";
import { WebSerialConnection } from "./web-serial.js";

const elements = {
  terminal: document.querySelector("#terminal"),
  themeSelect: document.querySelector("#themeSelect"),
  fontDecreaseButton: document.querySelector("#fontDecreaseButton"),
  fontIncreaseButton: document.querySelector("#fontIncreaseButton"),
  configButton: document.querySelector("#configButton"),
  configOverlay: document.querySelector("#configOverlay"),
  configPanel: document.querySelector("#configPanel"),
  configCloseButton: document.querySelector("#configCloseButton"),
  baudRateInput: document.querySelector("#baudRateInput"),
  dataBitsSelect: document.querySelector("#dataBitsSelect"),
  stopBitsSelect: document.querySelector("#stopBitsSelect"),
  paritySelect: document.querySelector("#paritySelect"),
  flowControlSelect: document.querySelector("#flowControlSelect"),
  enterModeSelect: document.querySelector("#enterModeSelect"),
  localEchoCheckbox: document.querySelector("#localEchoCheckbox"),
  connectButton: document.querySelector("#connectButton"),
  clearButton: document.querySelector("#clearButton"),
  downloadButton: document.querySelector("#downloadButton")
};

const SERIAL_BAUD_RATE = 115200;
const THEME_STORAGE_KEY = "bit_pirate_webserial_theme";
const FONT_SIZE_STORAGE_KEY = "bit_pirate_webserial_font_size";
const SERIAL_SETTINGS_STORAGE_KEY = "bit_pirate_webserial_serial_settings";
const DEFAULT_SERIAL_SETTINGS = Object.freeze({
  baudRate: SERIAL_BAUD_RATE,
  dataBits: 8,
  stopBits: 1,
  parity: "none",
  flowControl: "none",
  enterMode: "cr",
  localEcho: false
});
const WELCOME_TEXT =
`   ____  _ _     ____  _           _
  | __ )(_) |_  |  _ \\(_)_ __ __ _| |_ ___
  |  _ \\| | __| | |_) | | '__/ _\` | __/ _ \\
  | |_) | | |_  |  __/| | | | (_| | ||  __/
  |____/|_|\\__| |_|   |_|_|  \\__,_|\\__\\___|

      SERIAL ACCESS FROM YOUR BROWSER

  This page is a browser serial terminal
  for any USB serial device.

  Use the settings button to change
  baud rate, data bits, parity,
  stop bits, flow control, and Enter.

  1. Plug in your device by USB.
  2. Click Connect at the top right.
  3. Select the USB serial port.
  4. Click here and start typing.

  Web Serial gives this page direct
  USB serial access from a web browser.
  Nothing is sent to a server.
`;

const serial = new WebSerialConnection();
const terminal = new SerialTerminal(elements.terminal);

let connecting = false;
let connected = false;

function loadPreference(key, fallback) {
  try {
    return localStorage.getItem(key) || fallback;
  } catch {
    return fallback;
  }
}

function savePreference(key, value) {
  try {
    localStorage.setItem(key, value);
  } catch {
    // Preferences are optional; the terminal should keep working without them.
  }
}

function loadSerialSettings() {
  try {
    const settings = {
      ...DEFAULT_SERIAL_SETTINGS,
      ...JSON.parse(localStorage.getItem(SERIAL_SETTINGS_STORAGE_KEY) || "{}")
    };
    return normalizeSerialSettings(settings);
  } catch {
    return { ...DEFAULT_SERIAL_SETTINGS };
  }
}

function saveSerialSettings(settings) {
  savePreference(SERIAL_SETTINGS_STORAGE_KEY, JSON.stringify(normalizeSerialSettings(settings)));
}

function normalizeSerialSettings(settings) {
  const dataBits = [7, 8];
  const stopBits = [1, 2];
  const parities = ["none", "even", "odd"];
  const flowControls = ["none", "hardware"];
  const enterModes = ["cr", "lf", "crlf"];

  const baudRate = Number(settings.baudRate);

  return {
    baudRate: Number.isInteger(baudRate) && baudRate > 0 ? baudRate : DEFAULT_SERIAL_SETTINGS.baudRate,
    dataBits: dataBits.includes(Number(settings.dataBits)) ? Number(settings.dataBits) : DEFAULT_SERIAL_SETTINGS.dataBits,
    stopBits: stopBits.includes(Number(settings.stopBits)) ? Number(settings.stopBits) : DEFAULT_SERIAL_SETTINGS.stopBits,
    parity: parities.includes(settings.parity) ? settings.parity : DEFAULT_SERIAL_SETTINGS.parity,
    flowControl: flowControls.includes(settings.flowControl) ? settings.flowControl : DEFAULT_SERIAL_SETTINGS.flowControl,
    enterMode: enterModes.includes(settings.enterMode) ? settings.enterMode : DEFAULT_SERIAL_SETTINGS.enterMode,
    localEcho: Boolean(settings.localEcho)
  };
}

function readSerialSettingsForm() {
  return {
    baudRate: Number(elements.baudRateInput.value),
    dataBits: Number(elements.dataBitsSelect.value),
    stopBits: Number(elements.stopBitsSelect.value),
    parity: elements.paritySelect.value,
    flowControl: elements.flowControlSelect.value,
    enterMode: elements.enterModeSelect.value,
    localEcho: elements.localEchoCheckbox.checked
  };
}

function applySerialSettings(settings) {
  settings = normalizeSerialSettings(settings);
  elements.baudRateInput.value = String(settings.baudRate);
  elements.dataBitsSelect.value = String(settings.dataBits);
  elements.stopBitsSelect.value = String(settings.stopBits);
  elements.paritySelect.value = settings.parity;
  elements.flowControlSelect.value = settings.flowControl;
  elements.enterModeSelect.value = settings.enterMode;
  elements.localEchoCheckbox.checked = settings.localEcho;
  terminal.setEnterMode(settings.enterMode);
  terminal.setLocalEcho(settings.localEcho);
}

function saveCurrentSerialSettings() {
  const settings = normalizeSerialSettings(readSerialSettingsForm());
  saveSerialSettings(settings);
  terminal.setEnterMode(settings.enterMode);
  terminal.setLocalEcho(settings.localEcho);
  return settings;
}

function updateThemeSelectColor(theme) {
  elements.themeSelect.classList.remove("theme-soft", "theme-green", "theme-contrast");
  elements.themeSelect.classList.add(`theme-${theme}`);
}

function applyStoredPreferences() {
  const storedTheme = loadPreference(THEME_STORAGE_KEY, "soft");
  const storedFontSize = Number(loadPreference(FONT_SIZE_STORAGE_KEY, "14"));
  const theme = ["soft", "green", "contrast"].includes(storedTheme) ? storedTheme : "soft";

  elements.themeSelect.value = theme;
  updateThemeSelectColor(theme);
  terminal.setTheme(theme);
  terminal.setFontSize(storedFontSize);
}

function showWelcome() {
  terminal.clear();
  terminal.write(WELCOME_TEXT.replace(/\n/g, "\r\n"), { log: false });
}

function setConnectedUi(connected) {
  const supported = serial.isSupported();
  const connectLabel = !supported ? "Unsupported" : connected ? "Disconnect" : connecting ? "Connecting..." : "Connect";

  elements.connectButton.disabled = connecting || !supported;
  elements.connectButton.textContent = connectLabel;
  elements.connectButton.classList.toggle("primary", !connected);
  elements.configButton.disabled = connecting || connected;
  elements.configButton.classList.toggle("is-suggested", supported && !connecting && !connected);
}

async function connect() {
  if (connecting || connected) {
    return;
  }

  connecting = true;
  setConnectedUi(false);

  try {
    const settings = saveCurrentSerialSettings();
    elements.configOverlay.hidden = true;
    elements.configPanel.hidden = true;
    terminal.clear();
    terminal.clearLog();
    await serial.connect({
      baudRate: settings.baudRate,
      dataBits: settings.dataBits,
      stopBits: settings.stopBits,
      parity: settings.parity,
      flowControl: settings.flowControl
    });
    connected = true;
    terminal.focus();
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    terminal.write(`\r\n[Connection failed: ${message}]\r\n`, { log: false });
    connected = false;
  } finally {
    connecting = false;
    setConnectedUi(connected);
  }
}

function openConfig() {
  elements.configOverlay.hidden = false;
  elements.configPanel.hidden = false;
}

function closeConfig() {
  saveCurrentSerialSettings();
  elements.configOverlay.hidden = true;
  elements.configPanel.hidden = true;
  terminal.focus();
}

async function disconnect() {
  if (connecting) {
    return;
  }

  await serial.disconnect("Disconnected");
  connected = false;
  setConnectedUi(connected);
}

function initializeCompatibility() {
  if (serial.isSupported()) {
    return;
  }

  terminal.write("\r\n[Web Serial is not available.]\r\n", { log: false });
  terminal.write("[Use Chrome or Edge on HTTPS or localhost.]\r\n", { log: false });
  setConnectedUi(false);
}

terminal.onInput(async (data) => {
  try {
    await serial.write(data);
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    terminal.write(`\r\n[Write failed: ${message}]\r\n`, { log: false });
  }
});

serial.addEventListener("data", (event) => {
  terminal.write(event.detail);
});

serial.addEventListener("status", (event) => {
  const { state, message } = event.detail;
  connected = state === "connected";
  setConnectedUi(connected);

  if (state === "disconnected" && message !== "Disconnected") {
    terminal.write(`\r\n[${message}]\r\n`, { log: false });
  }
});

serial.addEventListener("error", (event) => {
  terminal.write(`\r\n[Serial error: ${event.detail.message}]\r\n`, { log: false });
});

elements.connectButton.addEventListener("click", () => {
  if (connected) {
    void disconnect();
  } else {
    void connect();
  }
});
elements.clearButton.addEventListener("click", () => {
  if (connected) {
    terminal.clear();
    terminal.clearLog();
  } else {
    showWelcome();
  }

  terminal.focus();
});
elements.downloadButton.addEventListener("click", () => terminal.downloadLog());
elements.baudRateInput.addEventListener("input", saveCurrentSerialSettings);
elements.themeSelect.addEventListener("change", () => {
  terminal.setTheme(elements.themeSelect.value);
  updateThemeSelectColor(elements.themeSelect.value);
  savePreference(THEME_STORAGE_KEY, elements.themeSelect.value);
  terminal.focus();
});
elements.fontDecreaseButton.addEventListener("click", () => {
  const fontSize = terminal.adjustFontSize(-1);
  savePreference(FONT_SIZE_STORAGE_KEY, String(fontSize));
  terminal.focus();
});
elements.fontIncreaseButton.addEventListener("click", () => {
  const fontSize = terminal.adjustFontSize(1);
  savePreference(FONT_SIZE_STORAGE_KEY, String(fontSize));
  terminal.focus();
});
elements.configButton.addEventListener("click", openConfig);
elements.configCloseButton.addEventListener("click", closeConfig);
elements.configOverlay.addEventListener("click", closeConfig);
elements.configPanel.addEventListener("change", saveCurrentSerialSettings);
document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && !elements.configPanel.hidden) {
    closeConfig();
  }
});

document.addEventListener("visibilitychange", () => {
  if (!document.hidden) {
    terminal.fit();
  }
});

window.addEventListener("beforeunload", () => {
  if (connected) {
    void serial.disconnect("Page closed");
  }
});

setConnectedUi(false);
terminal.setAutoScroll(true);
applyStoredPreferences();
applySerialSettings(loadSerialSettings());
showWelcome();
initializeCompatibility();
