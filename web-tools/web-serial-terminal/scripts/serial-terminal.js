const TERMINAL_THEMES = Object.freeze({
  soft: {
    background: "#111111",
    foreground: "#d8ddd8",
    cursor: "#00ffcc",
    cursorAccent: "#121212",
    selectionBackground: "#00ff0055",
    black: "#000000",
    red: "#ff5f5f",
    green: "#d8ddd8",
    yellow: "#ffdc7d",
    blue: "#5cb7ff",
    magenta: "#ff79c6",
    cyan: "#00ffcc",
    white: "#e0e0e0",
    brightBlack: "#666666",
    brightRed: "#ff8787",
    brightGreen: "#f0f3f0",
    brightYellow: "#ffe9a8",
    brightBlue: "#8fcaff",
    brightMagenta: "#ff9fda",
    brightCyan: "#8ffff0",
    brightWhite: "#ffffff"
  },
  green: {
    background: "#050805",
    foreground: "#00ff00",
    cursor: "#00ffcc",
    cursorAccent: "#050805",
    selectionBackground: "#00ff0055",
    black: "#000000",
    red: "#ff5f5f",
    green: "#00ff00",
    yellow: "#ffdc7d",
    blue: "#5cb7ff",
    magenta: "#ff79c6",
    cyan: "#00ffcc",
    white: "#d8ffd8",
    brightBlack: "#666666",
    brightRed: "#ff8787",
    brightGreen: "#75ff75",
    brightYellow: "#ffe9a8",
    brightBlue: "#8fcaff",
    brightMagenta: "#ff9fda",
    brightCyan: "#8ffff0",
    brightWhite: "#ffffff"
  },
  contrast: {
    background: "#000000",
    foreground: "#ffffff",
    cursor: "#ffffff",
    cursorAccent: "#000000",
    selectionBackground: "#ffffff55",
    black: "#000000",
    red: "#ff5555",
    green: "#50fa7b",
    yellow: "#f1fa8c",
    blue: "#8be9fd",
    magenta: "#ff79c6",
    cyan: "#8be9fd",
    white: "#f8f8f2",
    brightBlack: "#777777",
    brightRed: "#ff6e6e",
    brightGreen: "#69ff94",
    brightYellow: "#ffffa5",
    brightBlue: "#a4ffff",
    brightMagenta: "#ff92df",
    brightCyan: "#a4ffff",
    brightWhite: "#ffffff"
  }
});

const DEFAULT_THEME = Object.freeze(TERMINAL_THEMES.soft);
const DEFAULT_FONT_SIZE = 14;
const MIN_FONT_SIZE = 11;
const MAX_FONT_SIZE = 22;

export class SerialTerminal {
  constructor(element) {
    const terminalCtor = window.Terminal;
    const fitCtor = window.FitAddon?.FitAddon || window.FitAddon;

    if (!terminalCtor || !fitCtor) {
      throw new Error("xterm.js failed to load.");
    }

    this.element = element;
    this.fitAddon = new fitCtor();
    this.localEcho = false;
    this.autoScroll = true;
    this.enterMode = "cr";
    this.disposables = [];
    this.log = [];
    this.maxLogChunks = 20000;
    this.inputHandler = null;
    this.syncingScrollbar = false;
    this.pendingBottomScroll = false;

    this.term = new terminalCtor({
      allowProposedApi: false,
      convertEol: false,
      cursorBlink: true,
      cursorStyle: "block",
      disableStdin: false,
      fontFamily: 'Menlo, "Courier New", Courier, "Liberation Mono", monospace',
      fontSize: DEFAULT_FONT_SIZE,
      letterSpacing: 0,
      lineHeight: 1.2,
      scrollback: 10000,
      tabStopWidth: 8,
      theme: DEFAULT_THEME
    });

    this.term.loadAddon(this.fitAddon);
    this.term.open(element);
    this.createScrollbar();
    this.fit();

    this.disposables.push(this.term.onData((data) => this.handleInput(data)));
    this.disposables.push(this.term.onScroll(() => this.syncScrollbar()));
    this.disposables.push(this.term.onResize(() => this.syncScrollbar()));

    const resizeObserver = new ResizeObserver(() => this.fit());
    resizeObserver.observe(element);
    this.disposables.push({ dispose: () => resizeObserver.disconnect() });
  }

  onInput(handler) {
    this.inputHandler = handler;
  }

  write(text, { log = true } = {}) {
    if (log) {
      this.log.push(text);
      if (this.log.length > this.maxLogChunks) {
        this.log.splice(0, this.log.length - this.maxLogChunks);
      }
    }

    this.term.write(text, () => {
      if (this.autoScroll) {
        this.scheduleScrollToBottom();
      } else {
        this.syncScrollbar();
      }
    });
  }

  clear() {
    this.term.clear();
  }

  clearLog() {
    this.log = [];
  }

  focus() {
    this.term.focus();
  }

  createScrollbar() {
    this.scrollbar = document.createElement("div");
    this.scrollbar.className = "terminal-scrollbar";
    this.scrollbar.setAttribute("aria-hidden", "true");

    this.scrollbarTrack = document.createElement("div");
    this.scrollbarTrack.className = "terminal-scrollbar-track";

    this.scrollbar.appendChild(this.scrollbarTrack);
    this.element.appendChild(this.scrollbar);

    this.scrollbar.addEventListener("scroll", () => {
      if (this.syncingScrollbar) {
        return;
      }

      const maxScroll = this.term.buffer.active.baseY;
      if (maxScroll <= 0) {
        return;
      }

      this.term.scrollToLine(Math.round(this.scrollbar.scrollTop));
    });
  }

  syncScrollbar() {
    if (!this.scrollbar || !this.scrollbarTrack) {
      return;
    }

    requestAnimationFrame(() => {
      const maxScroll = this.term.buffer.active.baseY;
      const viewportY = this.term.buffer.active.viewportY;
      const visibleHeight = this.scrollbar.clientHeight;

      this.scrollbarTrack.style.height = `${visibleHeight + maxScroll}px`;
      this.syncingScrollbar = true;
      this.scrollbar.scrollTop = viewportY;
      this.syncingScrollbar = false;
    });
  }

  scrollToBottom() {
    this.term.scrollToBottom();
    this.forceViewportBottom();
    this.syncScrollbar();
  }

  scheduleScrollToBottom() {
    if (this.pendingBottomScroll) {
      return;
    }

    this.pendingBottomScroll = true;
    const run = () => {
      this.scrollToBottom();
      requestAnimationFrame(() => this.scrollToBottom());
      window.setTimeout(() => this.scrollToBottom(), 0);
      window.setTimeout(() => {
        this.scrollToBottom();
        this.pendingBottomScroll = false;
      }, 32);
    };

    requestAnimationFrame(run);
  }

  forceViewportBottom() {
    const viewport = this.element.querySelector(".xterm-viewport");
    if (viewport) {
      viewport.scrollTop = viewport.scrollHeight;
    }

    if (this.scrollbar) {
      this.syncingScrollbar = true;
      this.scrollbar.scrollTop = this.scrollbar.scrollHeight;
      this.syncingScrollbar = false;
    }
  }

  fit() {
    requestAnimationFrame(() => {
      try {
        this.fitAddon.fit();
        this.syncScrollbar();
      } catch {
        // Fit can run before xterm has measurable dimensions during startup.
      }
    });
  }

  setLocalEcho(enabled) {
    this.localEcho = enabled;
  }

  setAutoScroll(enabled) {
    this.autoScroll = enabled;
  }

  setEnterMode(mode) {
    this.enterMode = mode;
  }

  setTheme(themeName) {
    this.term.options.theme = TERMINAL_THEMES[themeName] || DEFAULT_THEME;
  }

  setFontSize(size) {
    const requestedSize = Number(size);
    const safeSize = Number.isFinite(requestedSize) ? requestedSize : DEFAULT_FONT_SIZE;
    const fontSize = Math.min(MAX_FONT_SIZE, Math.max(MIN_FONT_SIZE, safeSize));
    this.term.options.fontSize = fontSize;
    this.fit();
    return fontSize;
  }

  adjustFontSize(delta) {
    return this.setFontSize(this.term.options.fontSize + delta);
  }

  downloadLog(filenamePrefix = "web-serial-terminal") {
    const contents = this.log.join("");
    const stamp = new Date().toISOString().replace(/[:.]/g, "-");
    const blob = new Blob([contents], { type: "text/plain;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");

    link.href = url;
    link.download = `${filenamePrefix}-${stamp}.log`;
    link.click();

    setTimeout(() => URL.revokeObjectURL(url), 0);
  }

  handleInput(data) {
    const normalized = this.normalizeInput(data);

    if (this.localEcho && normalized) {
      this.write(normalized, { log: false });
    }

    if (this.inputHandler) {
      this.inputHandler(normalized);
    }
  }

  normalizeInput(data) {
    if (data !== "\r") {
      return data;
    }

    if (this.enterMode === "lf") {
      return "\n";
    }

    if (this.enterMode === "crlf") {
      return "\r\n";
    }

    return "\r";
  }

  dispose() {
    for (const disposable of this.disposables) {
      disposable.dispose();
    }
    this.term.dispose();
  }
}
