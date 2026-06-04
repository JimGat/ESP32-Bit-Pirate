#pragma once

inline const char* style_css = R"rawliteral(

/* =========================
   Global styles
   ========================= */
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
  overflow-wrap: break-word;
}

html, body {
  height: 100%;
  width: 100%;
  max-width: 100vw;
  max-height: 100dvh;
  font-family: Menlo, 'Courier New', Courier, 'Liberation Mono', monospace;
  background-color: #121212;
  color: #e0e0e0;
  overflow: hidden;
  touch-action: manipulation;
  position: fixed;
  top: 0;
  left: 0;
}

body {
  overscroll-behavior: none;
}

main {
  display: flex;
  flex-direction: column;
  flex-grow: 1;
  height: 100dvh;
  padding:
    max(10px, env(safe-area-inset-top))
    max(10px, env(safe-area-inset-right))
    max(10px, env(safe-area-inset-bottom))
    max(10px, env(safe-area-inset-left));
  gap: 10px;
  overflow: hidden;
  min-width: 0;
  max-width: 100%;
  position: relative;
}

@keyframes ai-soft-glow {
  0% {
    box-shadow:
      0 0 0 rgba(0, 255, 204, 0),
      0 0 8px rgba(0, 255, 204, 0.05),
      0 2px 10px rgba(0, 0, 0, 0.28);
  }
  50% {
    box-shadow:
      0 0 0 rgba(0, 255, 204, 0),
      0 0 14px rgba(0, 255, 204, 0.10),
      0 2px 10px rgba(0, 0, 0, 0.30);
  }
  100% {
    box-shadow:
      0 0 0 rgba(0, 255, 204, 0),
      0 0 8px rgba(0, 255, 204, 0.05),
      0 2px 10px rgba(0, 0, 0, 0.28);
  }
}

#ai-btn {
  position: absolute;
  top: 50%;
  right: 92px;
  transform: translateY(-50%);
  height: 28px;
  padding: 0 10px;
  font-size: 0.9rem;
  line-height: 28px;
  display: flex;
  align-items: center;
  gap: 4px;
  border: 1px solid #444;
  border-radius: 6px;
  background: linear-gradient(145deg, #2a2a2a, #1a1a1a);
  color: #ffdc7d;
  cursor: pointer;
  z-index: 2;
  animation: ai-soft-glow 7s ease-in-out infinite;
}

/* =========================
   Terminal output
   ========================= */
#output {
  font-family: Menlo, 'Courier New', Courier, 'Liberation Mono', monospace;
  font-size: 0.9rem;
  letter-spacing: 0;
  line-height: 1.2;
  flex-grow: 1;
  min-height: 0;
  width: 100%;
  background: linear-gradient(145deg, #1a1a1a, #0f0f0f);
  color: #00ff00;
  border: 1px solid #333;
  border-radius: 8px;
  padding: 12px;
  resize: none;
  box-shadow: inset 0 0 5px #000;
  overflow-y: auto;
  overflow-x: hidden;
  white-space: pre-wrap;
  word-break: break-word;
  caret-color: transparent;
}

#output:focus {
  outline: none;
  border-color: #00ffcc;
  box-shadow: 0 0 5px #00ffcc44;
}

#output::-webkit-scrollbar {
  display: none;
}

/* =========================
   Input area
   ========================= */
.input-area {
  display: flex;
  gap: 8px;
  width: 100%;
  flex-shrink: 0;
}

.input-wrap {
  position: relative;
  flex: 1;
  min-width: 0;
}

.input-wrap input[type="text"] {
  width: 100%;
  padding: 10px;
  padding-right: 160px;
  background-color: #1c1c1c;
  color: #00ff00;
  border: 1px solid #333;
  border-radius: 8px;
  font-size: 1rem;
  outline: none;
  min-width: 0;
}

input[type="text"]:focus,
#ai-prompt:focus,
.modal-body input[type="password"]:focus {
  border-color: #00ffcc;
  box-shadow: 0 0 5px #00ffcc44;
  outline: none;
}

#files-btn {
  position: absolute;
  top: 50%;
  right: 10px;
  transform: translateY(-50%);
  height: 28px;
  padding: 0 10px;
  font-size: 0.9rem;
  line-height: 28px;
  display: flex;
  align-items: center;
  gap: 4px;
  border: 1px solid #444;
  border-radius: 6px;
  background: linear-gradient(145deg, #2a2a2a, #1a1a1a);
  color: #ffdc7d;
  cursor: pointer;
}

#files-btn:hover {
  background: #333;
  box-shadow: 0 0 6px #00ff0044;
}

.actions {
  display: flex;
  flex-shrink: 0;
}

/* =========================
   Generic buttons
   ========================= */
button {
  padding: 10px 16px;
  background: linear-gradient(145deg, #2a2a2a, #1a1a1a);
  color: #00ff00;
  border: 1px solid #444;
  border-radius: 8px;
  cursor: pointer;
  font-size: 1rem;
  white-space: nowrap;
  transition: background 0.2s ease, box-shadow 0.2s ease, border-color 0.2s ease;
}

button#files-btn {
  padding: 0 10px;
  font-size: 0.9rem;
}

button:hover {
  background: #333;
  box-shadow: 0 0 6px #00ff0044;
}

/* =========================
   History
   ========================= */
#history-title {
  margin: 4px 0 0 0;
  font-size: 1rem;
  display: none;
  flex-shrink: 0;
}

.history-area {
  display: flex;
  flex-wrap: nowrap;
  gap: 6px;
  white-space: nowrap;
  padding-bottom: 4px;
  flex-shrink: 0;
  width: 100%;
  max-width: 100%;
  overflow-x: auto;
  overflow-y: hidden;
  min-width: 0;
  -webkit-overflow-scrolling: touch;
}

.history-area::-webkit-scrollbar {
  width: 0;
  height: 0;
}

.history-area button:hover {
  background: #2a2a2a;
}

.history-area::-webkit-scrollbar {
  height: 6px;
}

.history-area::-webkit-scrollbar-thumb {
  background-color: #444;
  border-radius: 4px;
}

.history-area::-webkit-scrollbar-track {
  background-color: #222;
}

.history-area button {
  flex: 0 0 auto;
  background: #1a1a1a;
  color: #00ff00;
  border: 1px solid #333;
  border-radius: 6px;
  padding: 6px 10px;
  font-size: 0.85em;
  transition: background 0.2s ease;
}

/* =========================
   Popup (WS Lost)
   ========================= */
.popup {
  position: fixed;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  background: #222;
  color: #ff5555;
  padding: 12px 18px;
  border: 1px solid #444;
  border-radius: 8px;
  box-shadow: 0 0 8px #000;
  z-index: 9999;
  font-size: 0.95rem;
  max-width: 90vw;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 6px;
  text-align: center;
  overflow: hidden;
}

.popup-main {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 10px;
  white-space: nowrap;
}

.popup-hint {
  color: #ddd;
  font-size: 0.85rem;
}

.popup a {
  color: #00ffcc;
  text-decoration: underline;
}

.popup a:hover {
  color: #00ffff;
}

/* =========================
   AI Panel Overlay
   ========================= */
.ai-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0,0,0,0.4);
  z-index: 9100;
  backdrop-filter: blur(2px);
}

/* =========================
   AI Panel
   ========================= */
.ai-panel {
  position: fixed;
  right: 12px;
  bottom: 12px;
  width: min(560px, 92vw);
  height: min(75vh, 680px);
  background: linear-gradient(145deg, #1b1b1b, #111111);
  border: 1px solid #333;
  border-radius: 10px;
  z-index: 9110;
  box-shadow: 0 8px 24px rgba(0,0,0,0.6);
  display: flex;
  flex-direction: column;
  gap: 10px;
  padding: 12px;
  overflow: hidden;
  scrollbar-width: thin;
  scrollbar-color: #666 #1a1a1a;
}

.ai-panel::-webkit-scrollbar {
  width: 8px;
}

.ai-panel::-webkit-scrollbar-track {
  background: #1a1a1a;
  border-radius: 10px;
}

.ai-panel::-webkit-scrollbar-thumb {
  background-color: #666;
  border-radius: 10px;
  border: 2px solid #1a1a1a;
}

.ai-panel::-webkit-scrollbar-thumb:hover {
  background-color: #888;
}

.ai-panel-header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 10px;
  flex-shrink: 0;
}

.ai-panel-header h2 {
  font-size: 1.05rem;
  color: #f2f2f2;
}

.ai-panel-header p {
  margin-top: 4px;
  font-size: 0.82rem;
  color: #9d9d9d;
}

.ai-close {
  background: #2a2a2a;
  color: #ccc;
  border: 1px solid #444;
  border-radius: 50%;
  width: 30px;
  height: 30px;
  font-size: 1rem;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0;
  flex-shrink: 0;
}

.ai-close:hover {
  background: #333;
  color: #fff;
}

.ai-block {
  display: flex;
  flex-direction: column;
  gap: 6px;
  flex-shrink: 0;
}

.ai-label {
  font-size: 0.82rem;
  color: #b8b8b8;
}

#ai-prompt {
  width: 100%;
  min-height: 110px;
  max-height: 220px;
  resize: vertical;
  background: #111;
  color: #e7e7e7;
  border: 1px solid #333;
  border-radius: 8px;
  padding: 10px;
  font-family: inherit;
  font-size: 0.95rem;
}

.ai-actions {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
  flex-shrink: 0;
}

.ai-actions button {
  flex: 1 1 140px;
}

#ai-translate-btn {
  color: #00ffcc;
}

#ai-settings-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 10px;

  min-width: 100px;
  height: 44px;

  padding: 0 12px;
  text-align: center;
}

#api-status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: red;
  box-shadow: 0 0 4px rgba(116, 6, 6, 0.61);
  flex-shrink: 0; 
}

#api-status-dot.ready {
  background: #00ff88;
  box-shadow: 0 0 6px rgba(0, 255, 136, 0.7);
}

.ai-status {
  display: none;
  flex-shrink: 0;
  padding: 10px 12px;
  border-radius: 8px;
  border: 1px solid #333;
  font-size: 0.85rem;
  line-height: 1.35;
  background: #171717;
  color: #ccc;
}

.ai-status-info {
  border-color: #35506a;
  background: #13202a;
  color: #9cc9f5;
}

.ai-status-success {
  border-color: #28563a;
  background: #112218;
  color: #8de0ac;
}

.ai-status-error {
  border-color: #6a2f2f;
  background: #261313;
  color: #ff9d9d;
}

.ai-results {
  flex: 1;
  min-height: 0;
  overflow: auto;
  display: flex;
  flex-direction: column;
  gap: 10px;
  padding-right: 2px;
}

.ai-results::-webkit-scrollbar {
  width: 8px;
}

.ai-results::-webkit-scrollbar-track {
  background: #121212;
  border-radius: 8px;
}

.ai-results::-webkit-scrollbar-thumb {
  background: #444;
  border-radius: 8px;
}

.ai-empty {
  color: #8b8b8b;
  font-size: 0.9rem;
  padding: 8px 2px;
}

.ai-card {
  background: linear-gradient(145deg, #121212, #0d0d0d);
  border: 1px solid #2d2d2d;
  border-radius: 8px;
  padding: 10px;
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.ai-card-title {
  font-size: 0.92rem;
  color: #f4f4f4;
  font-weight: 700;
}

.ai-card-command {
  font-family: inherit;
  font-size: 0.92rem;
  background: #0a0a0a;
  color: #00ff99;
  border: 1px solid #262626;
  border-radius: 6px;
  padding: 8px;
  white-space: pre-wrap;
  word-break: break-word;
}

.ai-card-description {
  font-size: 0.82rem;
  color: #a9a9a9;
  line-height: 1.4;
}

.ai-card-steps {
  display: flex;
  flex-direction: column;
  gap: 4px;
  font-size: 0.8rem;
  color: #bcbcbc;
  padding: 6px 8px;
  background: #141414;
  border: 1px solid #262626;
  border-radius: 6px;
}

.ai-card-actions {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
}

.ai-card-actions button {
  flex: 1 1 100px;
  font-size: 0.9rem;
  padding: 8px 10px;
}

.ai-notes {
  font-size: 0.82rem;
  line-height: 1.45;
  color: #b7b7b7;
  background: #141414;
  border: 1px solid #282828;
  border-radius: 8px;
  padding: 10px;
}

/* =========================
   Modal (API key)
   ========================= */
.modal-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0,0,0,0.45);
  backdrop-filter: blur(2px);
  z-index: 9400;
}

.modal {
  position: fixed;
  top: 50%;
  left: 50%;
  width: min(460px, calc(100vw - 24px));
  transform: translate(-50%, -50%);
  background: linear-gradient(145deg, #1c1c1c, #131313);
  border: 1px solid #333;
  border-radius: 10px;
  box-shadow: 0 10px 30px rgba(0,0,0,0.6);
  z-index: 9410;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.modal-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  padding: 12px 14px;
  border-bottom: 1px solid #2b2b2b;
}

.modal-header h2 {
  font-size: 1rem;
  color: #f1f1f1;
}

.modal-close {
  background: #2a2a2a;
  color: #ccc;
  border: 1px solid #444;
  border-radius: 50%;
  width: 30px;
  height: 30px;
  font-size: 1rem;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0;
}

.modal-close:hover {
  background: #333;
  color: #fff;
}

.modal-body {
  display: flex;
  flex-direction: column;
  gap: 10px;
  padding: 14px;
}

.modal-text {
  font-size: 0.84rem;
  color: #a9a9a9;
  line-height: 1.45;
}

.modal-body input[type="password"] {
  width: 100%;
  padding: 10px 12px;
  background: #111;
  color: #f0f0f0;
  border: 1px solid #333;
  border-radius: 8px;
  font-size: 0.95rem;
  font-family: inherit;
}

.modal-actions {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
}

.modal-actions button {
  flex: 1 1 140px;
}

/* =========================
   File Panel
   ========================= */
.fp-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0,0,0,0.4);
  z-index: 9000;
  backdrop-filter: blur(2px);
}

.fp {
  position: fixed;
  right: 12px;
  bottom: 12px;
  width: min(560px, 92vw);
  height: min(70vh, 560px);
  background: #1a1a1a;
  border: 1px solid #333;
  border-radius: 10px;
  z-index: 9010;
  box-shadow: 0 8px 24px rgba(0,0,0,0.6);
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding: 10px;
  scrollbar-width: thin;
  scrollbar-color: #666 #1a1a1a;
}

.fp::-webkit-scrollbar {
  width: 8px;
}

.fp::-webkit-scrollbar-track {
  background: #1a1a1a;
  border-radius: 10px;
}

.fp::-webkit-scrollbar-thumb {
  background-color: #666;
  border-radius: 10px;
  border: 2px solid #1a1a1a;
}

.fp::-webkit-scrollbar-thumb:hover {
  background-color: #888;
}

.fp-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.fp-header h2 {
  font-size: 1.05rem;
}

.fp-close {
  background: #2a2a2a;
  color: #ccc;
  border: 1px solid #444;
  border-radius: 50%;
  width: 28px;
  height: 28px;
  font-size: 1rem;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0;
}

.fp-close:hover {
  background: #333;
  color: #fff;
}

.fp-drop {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 6px;
  border: 1px dashed #444;
  border-radius: 8px;
  padding: 14px;
  text-align: center;
  color: #bbb;
  background: #161616;
}

.fp-drop.drag {
  border-color: #00ffcc;
  box-shadow: 0 0 6px #00ffcc44;
}

.fp-drop.disabled {
  opacity: 0.55;
  pointer-events: none;
}

.fp-upload-btn {
  display: inline-block;
  padding: 8px 10px;
  background: #2a2a2a;
  border: 1px solid #444;
  border-radius: 8px;
  cursor: pointer;
  color: #00ff00;
}

.fp-upload-btn input {
  display: none;
}

.fp-refresh {
  padding: 6px 10px;
}

.fp-list-header {
  font-size: 0.85rem;
  color: #bbb;
  padding: 2px 2px 0 2px;
}

.fp-list {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(140px, 1fr));
  gap: 10px;
  overflow: auto;
  flex: 1;
  border: 1px solid #222;
  border-radius: 8px;
  padding: 10px;
  background: #121212;
  grid-auto-rows: 150px;
}

.fp-empty {
  color: #999;
  font-size: 0.9rem;
  padding: 10px;
}

/* File item */
.fp-item {
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  gap: 6px;
  background: #181818;
  border: 1px solid #2a2a2a;
  border-radius: 8px;
  padding: 8px;
  height: 100%;
  min-height: 0;
}

.fp-icon {
  font-size: 28px;
  text-align: center;
}

.fp-name {
  font-size: 0.9rem;
  text-align: center;
  color: #e0e0e0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  padding: 5px 0;
}

.fp-meta {
  font-size: 0.8rem;
  text-align: center;
  color: #aaa;
}

/* File actions */
.fp-actions {
  display: flex;
  gap: 6px;
  justify-content: center;
  margin-top: 4px;
}

.fp-dl {
  padding: 6px 8px;
  font-size: 0.85rem;
  color: #7dd3fc;
  border: 1px solid #264653;
  background: #10222b;
  border-radius: 6px;
}

.fp-dl:hover {
  filter: brightness(1.1);
}

.fp-del {
  align-self: center;
  padding: 6px 8px;
  font-size: 0.85rem;
  color: #ff6666;
  border: 1px solid #553;
  background: #2a1515;
  border-radius: 6px;
}

.fp-del:hover {
  filter: brightness(1.1);
}

/* =========================
   Responsive
   ========================= */
@media (max-width: 600px), (max-height: 600px) {
  main {
    padding: 8px;
    gap: 6px;
  }

  #output {
    font-size: 0.8rem;
    padding: 8px;
  }

  .input-area {
    flex-direction: column;
    gap: 6px;
  }

  input[type="text"],
  #ai-prompt,
  .modal-body input[type="password"] {
    font-size: 0.95rem;
    width: 100%;
  }

  #send-btn,
  .ai-card-actions button,
  .modal-actions button {
    width: 100%;
    flex: 1 1 100%;
    font-size: 0.95rem;
  }

  #ai-btn,
  #files-btn {
    width: 38px;
    min-width: 38px;
    height: 32px;
    padding: 0;
    font-size: 0;
    line-height: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 0;
    overflow: hidden;
    white-space: nowrap;
    text-indent: -9999px;
  }

  #ai-btn::before {
    content: "✨";
    font-size: 1rem;
    line-height: 1;
    text-indent: 0;
  }

  #files-btn::before {
    content: "📁";
    font-size: 1rem;
    line-height: 1;
    text-indent: 0;
  }

  .ai-card-actions {
    display: flex;
    flex-wrap: nowrap;
    gap: 6px;
  }

  .ai-card-actions button {
    flex: 1 1 0;
    min-width: 0;
    width: auto;
    height: 34px;
    padding: 0;
    font-size: 0;
    line-height: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 0;
    overflow: hidden;
    white-space: nowrap;
    text-indent: -9999px;
  }

  .ai-card-actions button::before {
    text-indent: 0;
    display: block;
    font-size: 1rem;
    line-height: 1;
  }

  .ai-card-actions button:nth-child(1)::before {
    content: "📋";
  }

  .ai-card-actions button:nth-child(2)::before {
    content: "⌨️";
  }

  .ai-card-actions button:nth-child(3)::before {
    content: "📨";
  }

  #ai-btn {
    right: 54px;
  }

  #files-btn {
    right: 10px;
  }

  .input-wrap input[type="text"] {
    padding-right: 100px;
  }
    
  .history-area {
    gap: 4px;
  }

  .history-area button {
    font-size: 0.75em;
    padding: 5px 8px;
  }

  .ai-panel {
    right: 8px;
    left: 8px;
    bottom: 8px;
    width: auto;
    height: min(78vh, 640px);
    padding: 10px;
  }

  .ai-actions {
    display: flex;
    flex-wrap: nowrap;
    gap: 6px;
  }

  .ai-actions button {
    width: auto;
    flex: 1 1 0;
    min-width: 0;
    font-size: 0.9rem;
    padding: 10px 8px;
  }

  .fp {
    right: 8px;
    left: 8px;
    width: auto;
    height: min(70vh, 520px);
  }

  .fp-list {
    grid-auto-rows: 150px;
  }

  .modal {
    width: calc(100vw - 16px);
  }
}
)rawliteral";
