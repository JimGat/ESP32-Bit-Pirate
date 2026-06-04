#pragma once

inline const char* index_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 Bit Pirate</title>
  <link rel="stylesheet" href="/style.css">
  <link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Ctext x='50%25' y='50%25' text-anchor='middle' dominant-baseline='central' font-family='sans-serif' font-size='56' font-weight='900' fill='%2300ff00'%3EB%3C/text%3E%3C/svg%3E">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover, user-scalable=no, interactive-widget=resizes-content">
</head>
<body>

  <!-- Main Terminal Area -->
  <main>
    <!-- Output area -->
    <textarea id="output" readonly></textarea>

    <!-- Input area -->
    <div class="input-area">
      <div class="input-wrap">
        <input
          type="text"
          id="command"
          placeholder="Enter command"
          autocapitalize="off"
          autocomplete="off"
          autocorrect="off"
          spellcheck="false">
        <button id="ai-btn" type="button" title="Open Pirate Assistant">✨ AI</button>
        <button id="files-btn" type="button" title="Manage files">📁 Files</button>
      </div>
      <div class="actions">
        <button id="send-btn" type="button">📨 Send</button>
      </div>
    </div>

    <div id="history" class="history-area"></div>
  </main>

  <!-- WebSocket Lost -->
  <div id="ws-lost-popup" class="popup" style="display: none;">
    <div class="popup-main">
      <span class="popup-text">Connection lost.</span>
      <a href="#" onclick="location.reload()">Refresh</a>
    </div>
    <div class="popup-hint">One terminal session only</div>
  </div>

  <!-- AI Panel -->
  <div id="ai-panel-overlay" class="ai-overlay" style="display:none;"></div>
  <aside id="ai-panel" class="ai-panel" style="display:none;">
    <div class="ai-panel-header">
      <div>
        <h2>🏴‍☠️ Pirate Assistant</h2>
      </div>
      <button class="ai-close" type="button" aria-label="Close" onclick="closeAiPanel()">×</button>
    </div>

    <div class="ai-block">
      <label class="ai-label" for="ai-prompt">What do you want to do?</label>
      <textarea
        id="ai-prompt"
        placeholder="Example: How can I read an AT24C EEPROM?"
        autocapitalize="off"
        autocomplete="off"
        autocorrect="off"
        spellcheck="false"></textarea>
    </div>

    <div class="ai-actions">
      <button id="ai-translate-btn" type="button">Translate</button>
      <button id="ai-settings-btn">
        API Key
        <span id="api-status-dot"></span>
      </button>
    </div>

    <div id="ai-results" class="ai-results">
      <div class="ai-empty">No suggestions yet.</div>
    </div>
  </aside>

  <!-- API key modal -->
  <div id="api-key-overlay" class="modal-overlay" style="display:none;"></div>
  <div id="api-key-modal" class="modal" style="display:none;">
    <div class="modal-header">
      <h2>Gemini API Key</h2>
      <button class="modal-close" type="button" aria-label="Close" onclick="closeApiKeyModal()">×</button>
    </div>

    <div class="modal-body">
      <p class="modal-text">
        Please provide your Gemini API key, it will be saved locally. See <a href="https://ai.google.dev/gemini-api/docs/quickstart" target="_blank">Gemini API Quickstart</a>.
      </p>

      <label class="ai-label" for="gemini-api-key">API Key</label>
      <input
        type="password"
        id="gemini-api-key"
        placeholder="Paste Gemini API key here"
        autocapitalize="off"
        autocomplete="off"
        autocorrect="off"
        spellcheck="false">

      <div class="modal-actions">
        <button id="save-api-key-btn" type="button">Save locally</button>
        <button id="clear-api-key-btn" type="button">Clear</button>
      </div>
    </div>
  </div>

  <!-- File Panel -->
  <div id="file-panel-overlay" class="fp-overlay" style="display:none;"></div>
  <div id="file-panel" class="fp" style="display:none;"
       ondragover="onDropOver(event)"
       ondragleave="onDropLeave(event)"
       ondrop="onDrop(event)">
    <div class="fp-header">
      <h2 id="fp-header-title">📁 LittleFS</h2>
      <button class="fp-close" type="button" aria-label="Close" onclick="closeFilePanel()">×</button>
    </div>

    <div id="dropzone" class="fp-drop"
         ondragover="onDropOver(event)"
         ondragleave="onDropLeave(event)"
         ondrop="onDrop(event)">
      <p>Drag & drop a file here, or</p>
      <label class="fp-upload-btn">
        <input id="file-input" type="file" onchange="onFileInput(event)" />
        Choose file
      </label>
    </div>

    <div class="fp-list-header">
      <span id="fp-space">Loading...</span>
    </div>
    <div id="file-list" class="fp-list"></div>
  </div>

  <script src="/scripts.js"></script>
</body>
</html>
)rawliteral";