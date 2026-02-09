#include "UartController.h"
#include <map>

/*
Constructor
*/
UartController::UartController(
    ITerminalView& terminalView,
    IInput& terminalInput,
    IInput& deviceInput,
    UartService& uartService,
    SdService& sdService,
    HdUartService& hdUartService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    UartAtShell& uartAtShell,
    HelpShell& helpShell,
    UartEmulationShell& uartEmulationShell
)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      deviceInput(deviceInput),
      uartService(uartService),
      sdService(sdService),
      hdUartService(hdUartService),
      argTransformer(argTransformer),
      userInputManager(userInputManager),
      uartAtShell(uartAtShell),
      helpShell(helpShell),
      uartEmulationShell(uartEmulationShell)
{}


/*
Entry point for command
*/
void UartController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "autobaud") handleAutoBaud();
    else if (cmd.getRoot() == "scan") handleScan();
    else if (cmd.getRoot() == "ping") handlePing();
    else if (cmd.getRoot() == "read") handleRead();
    else if (cmd.getRoot() == "write") handleWrite(cmd);
    else if (cmd.getRoot() == "bridge") handleBridge();
    else if (cmd.getRoot() == "at") handleAtCommand(cmd);
    else if (cmd.getRoot() == "emulator") handleEmulation();
    else if (cmd.getRoot() == "trigger") handleTrigger(cmd);
    else if (cmd.getRoot() == "spam") handleSpam(cmd);
    else if (cmd.getRoot() == "glitch") handleGlitch();
    else if (cmd.getRoot() == "xmodem") handleXmodem(cmd);
    else if (cmd.getRoot() == "swap") handleSwap();
    else if (cmd.getRoot() == "config") handleConfig();
    else handleHelp();
}

/*
Entry point for instructions
*/
void UartController::handleInstruction(const std::vector<ByteCode>& bytecodes) {
    auto result = uartService.executeByteCode(bytecodes);
    terminalView.println("");
    terminalView.print("UART Read: ");
    if (!result.empty()) {
        terminalView.println("");
        terminalView.println("");
        terminalView.println(result);
        uartService.clearUartBuffer();
    } else {
        terminalView.print("No data");
    }
    terminalView.println("");
}

/*
Bridge
*/
void UartController::handleBridge() {
    terminalView.println("Uart Bridge: In progress... Press [ANY ESP32 BUTTON] to stop.\n");
    while (true) {
        // Read from UART and print to terminal
        if (uartService.available()) {
            char c = uartService.read();
            terminalView.print(std::string(1, c));
        }

        // Read from user input and write to UART
        char c = terminalInput.readChar();
        if (c != KEY_NONE) {
            uartService.write(c);
        }
        
        // Read from device input and stop bridge if any
        c = deviceInput.readChar();
        if (c != KEY_NONE) {  
            terminalView.println("\nUart Bridge: Stopped by user.");
            break;
        }
    }
}

/*
Read
*/
void UartController::handleRead() {
    terminalView.println("UART Read: Streaming until [ENTER] is pressed...");
    uartService.flush();

    while (true) {
        // Stop if ENTER is pressed
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("");
            terminalView.println("UART Read: Stopped by user.");
            break;
        }

        // Print UART data as it comes
        while (uartService.available() > 0) {
            char c = uartService.read();
            terminalView.print(std::string(1, c));
        }
    }
}

/*
AT Command shell
*/
void UartController::handleAtCommand(const TerminalCommand& cmd) {
        uartAtShell.run();
}

/*
Write
*/
void UartController::handleWrite(TerminalCommand cmd) {
    std::string raw = cmd.getSubcommand() + cmd.getArgs();
    std::string decoded = argTransformer.decodeEscapes(raw);
    uartService.print(decoded);
    terminalView.println("UART Write: Text sent at baud " + std::to_string(state.getUartBaudRate()));
}

/*
Ping
*/
void UartController::handlePing() { 
    std::string response;
    unsigned long start = millis();
    size_t probeIndex = 0;

    terminalView.println("UART Ping: Probing for 5 seconds...");
    uartService.clearUartBuffer();

    while (millis() - start < 5000) {
        // Envoi progressif
        if (probeIndex < probes.size()) {
            uartService.write(probes[probeIndex]);
            probeIndex++;
        }

        // Lecture continue
        char c;
        while (uartService.available() > 0) {
            c = uartService.read();
            response += c;
        }

        delay(10);
    }

    // Analyse ASCII simple
    size_t asciiCount = 0;
    std::string result = "";
    for (char c : response)
        if (isprint(static_cast<unsigned char>(c)) || isspace(c)) {
            asciiCount++;
            result += c;
        }

    if (asciiCount < 5) {
        terminalView.println("UART Ping: No response.");
        return;
    }

    terminalView.println("UART Response: ");
    terminalView.println("");
    terminalView.println(result);
    terminalView.println("");

    terminalView.println("UART Ping: Device detected");
}

/*
Scan
*/
void UartController::handleScan() {
    // Default pins to scan
    std::vector<uint8_t> defaultPins = state.getJtagScanPins();

    // Protected pins to avoid
    const std::vector<uint8_t> protectedPins = state.getProtectedPins();

    // Ask user for pin group
    std::vector<uint8_t> selectedPins = userInputManager.readValidatedPinGroup(
        "GPIOs to scan for UART",
        defaultPins,
        protectedPins
    );

    // Limit selection to 8 pins max
    if (selectedPins.size() > 8) {
        terminalView.println("Too many pins selected, limiting to first 8.");
        selectedPins.resize(8);
    }
    
    // Accumulateur for activity
    std::map<uint8_t, UartService::PinActivity> accum;

    terminalView.println("UART Scan: Measuring edges on pins... Press [ENTER] to stop.\n");
    terminalView.println("[INFO]");
    terminalView.println("  The UART scan passively monitors GPIO pins to detect");
    terminalView.println("  UART-like electrical activity (edges and timings).");
    terminalView.println("  Pins showing activity are potential UART lines.");
    terminalView.println("");
    delay(300); // since the loop below is fast, the message above may not be seen without delay
    
    unsigned long lastPrint = millis();
    while (true) {

        // Stop if ENTER pressed
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("UART Scan: Stopped by user.");
            break;
        }

        // Measure activity for selected pins
        auto results = uartService.scanUartActivity(selectedPins, 30, 10, true);

        // Accumulate
        for (auto& r : results) {
            auto& a = accum[r.pin];
            a.pin = r.pin;
            a.edges += r.edges;
            a.edgesPerSec += r.edgesPerSec;   // rough sum
            if (r.approxBaud != 0)
                a.approxBaud = r.approxBaud; // keep last valid
        }

        // Periodic accumulated report
        unsigned long now = millis();
        if (now - lastPrint >= 1000) {
            lastPrint = now;

            terminalView.println("Active pins:");

            if (accum.empty()) {
                terminalView.println("  (none)");
            } else {
                for (auto& [pin, r] : accum) {
                    terminalView.println(
                        "  GPIO " + std::to_string(pin) +
                        " | edges=" + std::to_string(r.edges) +
                        " | approxBaud=" + std::to_string(r.approxBaud)
                    );
                }
            }

            terminalView.println("");
            accum.clear();
        }
    }
}

/*
Autobaud
*/
void UartController::handleAutoBaud() {
    uint8_t rxPin = state.getUartRxPin();
    terminalView.println("UART Autobaud: Listening on RX GPIO " + std::to_string(rxPin) + " until detected..." + " Press [ENTER] to stop\n");
    uartService.clearUartBuffer();

    while (true) {
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("UART Autobaud: Stopped by user.");
            return;
        }

        uint32_t baud = uartService.detectBaudByEdge(
            rxPin,
            30,     // totalMs
            10,    // windowMs
            5,     // minEdges
            true  // pullup
        );

        if (baud != 0) {
            terminalView.println("UART Autobaud: Baudrate detected " + std::to_string(baud));
            auto confirm = userInputManager.readYesNo("Save baudrate to config?", true);
            if (confirm) {
                state.setUartBaudRate(baud);
                uartService.switchBaudrate(baud);
                terminalView.println("UART Autobaud: Baudrate saved to config.\n");
            } 
            return;
        }
    }

    terminalView.println("UART Autobaud: Detection finished.");
    terminalView.println("");
}

/*
Spam
*/
void UartController::handleSpam(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || cmd.getArgs().empty()) {
        terminalView.println("Usage: spam <text> <ms>");
        return;
    }

    // Find the last space to separate SSID and password
    std::string full = cmd.getSubcommand() + " " + cmd.getArgs();
    size_t pos = full.find_last_of(' ');
    if (pos == std::string::npos || pos == full.size() - 1) {
        terminalView.println("Usage: spam <text> <ms>");
        return;
    }
    auto textRaw = full.substr(0, pos);
    auto msRaw = full.substr(pos + 1);

    std::string text = argTransformer.decodeEscapes(textRaw);

    if (!argTransformer.isValidNumber(msRaw)) {
        terminalView.println("Usage: spam <text> <ms>");
        return;
    }

    uint32_t delayMs = argTransformer.toUint32(msRaw);
    unsigned long lastSend = 0;

    terminalView.println(
        "UART Spam: Sending \"" + text + 
        "\" every " + std::to_string(delayMs) + 
        " ms at baud " + std::to_string(state.getUartBaudRate()) + 
        "... Press [ENTER] to stop."
    );

    while (true) {
        // Stop if ENTER pressed
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("\nUART Spam: Stopped by user.");
            break;
        }

        // Send if delay elapsed
        unsigned long now = millis();
        if (now - lastSend >= delayMs) {
            uartService.print(text);
            lastSend = now;
        }

        delay(1);
    }
}

/*
Xmodem
*/
void UartController::handleXmodem(const TerminalCommand& cmd) {
    std::string action = cmd.getSubcommand();
    std::string path = cmd.getArgs();

    if (action.empty()) {
        terminalView.println("Usage: xmodem <recv/send> <path>");
        return;
    }

    if (path.empty()) {
        terminalView.println("Error: missing path argument (ex: /file.txt)");
        return;
    }
    
    // Normalize path
    if (!path.empty() && path[0] != '/') {
        path = "/" + path;
    }

    terminalView.println("\nXmodem Configuration:");

    // Xmodem block size
    uint8_t defaultBlockSize = uartService.getXmodemBlockSize();
    uint8_t blockSize = userInputManager.readValidatedUint8("Block size (typ. 128 or 1024)", defaultBlockSize, 1, 128);
    uartService.setXmodemBlockSize(blockSize);

    // Xmodem id size
    uint8_t defaultIdSize = uartService.getXmodemIdSize();
    uint8_t idSize = userInputManager.readValidatedUint8("Block ID size in bytes (1-4)", defaultIdSize, 1, 4);
    uartService.setXmodemIdSize(idSize);

    // Xmodem CRC
    bool useCrc = userInputManager.readYesNo("Use CRC?", true);
    uartService.setXmodemCrc(useCrc);

    terminalView.println("\nXmodem configured\n");

    if (action == "recv") {
        handleXmodemReceive(path);
    } else if (action == "send") {
        handleXmodemSend(path);
    } else {
        terminalView.println("Usage: xmodem <recv/send> <path>");
    }
}

void UartController::handleXmodemSend(const std::string& path) {
    // Open SD with SPI pin
    auto sdMounted = sdService.configure(state.getSpiCLKPin(), state.getSpiMISOPin(), 
                    state.getSpiMOSIPin(), state.getSpiCSPin());

    //Check SD mounted
    if (!sdMounted) {
        terminalView.println("UART XMODEM: No SD card detected. Check SPI pins");
        return;
    }

    // Open the file
    File file = sdService.openFileRead(path);
    if (!file) {
        terminalView.println("UART XMODEM: Could not open file");
        return;
    }

    // Infos
    terminalView.println(" [INFO]  No progress bar will be shown on WEBUI.");
    terminalView.println("         Progress bar is only visible over USB Serial.");
    terminalView.println("         Please be patient during the transfer.\n");
    std::stringstream ss;
    ss << "         Estimated duration: ~" 
    << (uint32_t)((file.size() * 10.0) / state.getUartBaudRate()) 
    << " seconds.\n";
    terminalView.println(ss.str());

    // Send it
    terminalView.println("UART XMODEM: Sending...");
    bool ok = uartService.xmodemSendFile(file);
    file.close();
    sdService.end();

    // Result
    terminalView.println(ok ? "\nUART XMODEM: Success, file is transferred" : "\nUART XMODEM: Failed to transfer file");

    // Close Xmodem
    uartService.end();
    ensureConfigured();

    // Close SD
    sdService.end();
}

void UartController::handleXmodemReceive(const std::string& path) {
    // Open sd card with SPI pin
    auto sdMounted = sdService.configure(state.getSpiCLKPin(), state.getSpiMISOPin(), 
                    state.getSpiMOSIPin(), state.getSpiCSPin());

    //Check SD mounted
    if (!sdMounted) {
        terminalView.println("UART XMODEM: No SD card detected. Check SPI pins");
        return;
    }

    // Create target file
    File file = sdService.openFileWrite(path);
    if (!file) {
        terminalView.println("UART XMODEM: Could not create file.");
        return;
    }

    // Infos
    terminalView.println("");
    terminalView.println("  [INFO] XMODEM receive mode is blocking.");
    terminalView.println("         No progress bar will be shown on WEBUI.");
    terminalView.println("         Progress bar is only visible over USB Serial.");
    terminalView.println("         The device will wait for incoming data");
    terminalView.println("         for up to 2 minutes. Once started,");
    terminalView.println("         the transfer must complete before exiting.\n");

    // Receive
    terminalView.println("UART XMODEM: Receiving...");
    bool ok = uartService.xmodemReceiveToFile(file);
    file.close();
    sdService.end();

    // Result
    terminalView.println("");
    terminalView.println(ok ? 
        ("UART XMODEM: Receive OK, File saved to " + path) :
        "UART XMODEM: Receive failed");

    // Close Xmodem
    uartService.end();
    ensureConfigured();

    // Close SD
    if (!ok) {
        sdService.deleteFile(path); // delete created file
    }
    sdService.end();
}

/*
Config
*/
void UartController::handleConfig() {
    terminalView.println("UART Configuration:");

    const auto& forbidden = state.getProtectedPins();

    uint8_t rxPin = userInputManager.readValidatedPinNumber("RX pin number", state.getUartRxPin(), forbidden);
    state.setUartRxPin(rxPin);

    uint8_t txPin = userInputManager.readValidatedPinNumber("TX pin number", state.getUartTxPin(), forbidden);
    state.setUartTxPin(txPin);

    uint32_t baud = userInputManager.readValidatedUint32("Baud rate", state.getUartBaudRate());
    state.setUartBaudRate(baud);

    uint8_t dataBits = userInputManager.readValidatedUint8("Data bits (5-8)", state.getUartDataBits(), 5, 8);
    state.setUartDataBits(dataBits);

    char defaultParity = state.getUartParity().empty() ? 'N' : state.getUartParity()[0];
    char parityChar = userInputManager.readCharChoice("Parity (N/E/O)", defaultParity, {'N', 'E', 'O'});
    state.setUartParity(std::string(1, parityChar));

    uint8_t stopBits = userInputManager.readValidatedUint8("Stop bits (1 or 2)", state.getUartStopBits(), 1, 2);
    state.setUartStopBits(stopBits);

    bool inverted = userInputManager.readYesNo("Inverted?", state.isUartInverted());
    state.setUartInverted(inverted);

    uint32_t config = uartService.buildUartConfig(dataBits, parityChar, stopBits);
    state.setUartConfig(config);
    uartService.configure(baud, config, rxPin, txPin, inverted);

    terminalView.println("UART configured.");
    terminalView.println("");
}

/*
Help
*/
void UartController::handleHelp() {
    terminalView.println("\nUnknown command. Available UART commands:");
    helpShell.run(state.getCurrentMode(), false);
}

/*
Glitch
*/
void UartController::handleGlitch() {
    terminalView.println("Uart Glicher: Not Yet Implemented");
}

/* 
Swap pins
*/
void UartController::handleSwap() {
    uint8_t rx = state.getUartRxPin();
    uint8_t tx = state.getUartTxPin();

    // Swap in state
    state.setUartRxPin(tx);
    state.setUartTxPin(rx);

    // Reconfigure UART with swapped pins
    uartService.end();

    uint32_t baud = state.getUartBaudRate();
    uint32_t config = state.getUartConfig();
    bool inverted = state.isUartInverted();

    uartService.configure(baud, config, state.getUartRxPin(), state.getUartTxPin(), inverted);

    terminalView.println(
        "UART Swap: RX/TX swapped. RX=" + std::to_string(state.getUartRxPin()) +
        " TX=" + std::to_string(state.getUartTxPin())
    );
    terminalView.println("");
}

/*
Emulation
*/
void UartController::handleEmulation() {
    uartEmulationShell.run();
}

/*
Trigger
*/
void UartController::handleTrigger(const TerminalCommand& cmd) {
    ensureConfigured();

    // trigger [pattern]
    std::string responseRaw;
    std::string patternRaw = cmd.getSubcommand();
    if (!cmd.getArgs().empty()) {
        patternRaw += " " + cmd.getArgs();
    }

    // If missing pattern
    if (cmd.getSubcommand().empty()) {
        terminalView.println("");
        terminalView.println("UART Trigger: Send a response when a specific pattern is detected.");
        patternRaw = userInputManager.readString("\nThe pattern to be detected", "Hit ESC key");
    }

    terminalView.println("");
    terminalView.println("[Special characters reference]");
    terminalView.println("  ESC        = \\x1B");
    terminalView.println("  ENTER (CR) = \\r");
    terminalView.println("  ENTER (LF) = \\n");
    terminalView.println("  TAB        = \\t");
    terminalView.println("  BACKSPACE  = \\x08");
    terminalView.println("  DELETE     = \\x7F");
    terminalView.println("  SPACE      = \\x20");
    terminalView.println("  F1  = \\x1BOP    F2  = \\x1BOQ");
    terminalView.println("  F3  = \\x1BOR    F4  = \\x1BOS");
    terminalView.println("  F5  = \\x1B[15~  F6  = \\x1B[17~");
    terminalView.println("  F7  = \\x1B[18~  F8  = \\x1B[19~");
    terminalView.println("  F9  = \\x1B[20~  F10 = \\x1B[21~");
    terminalView.println("  F11 = \\x1B[23~  F12 = \\x1B[24~");
    terminalView.println("");
    responseRaw = userInputManager.readString("The response to be send", "\\x1B"); // default to ESC

    std::string textPattern;
    std::vector<uint8_t> hexPattern;
    std::vector<uint8_t> hexMask;
    bool isHex = false;

    if (!argTransformer.parsePattern(patternRaw, textPattern, hexPattern, hexMask, isHex)) {
        terminalView.println("UART Trigger: Invalid pattern. Use text or hex{7E 01 ?? 10}");
        return;
    }

    std::string response = argTransformer.decodeEscapes(responseRaw);
    if (response.empty()) {
        terminalView.println("UART Trigger: Empty response after decoding.\n");
        return;
    }

    terminalView.println("");
    terminalView.println("UART Trigger: Listening for pattern... Press [ENTER] to stop.");
    terminalView.println("");
    terminalView.println("  Pattern to detect : " + patternRaw);
    terminalView.println("  Response to send  : " + responseRaw);
    terminalView.println("");

    const size_t MAX_BUF = 512;
    const uint32_t COOLDOWN_MS = 20;
    uint32_t lastFire = 0;
    std::vector<uint8_t> buf;
    buf.reserve(MAX_BUF);

    while (true) {
        // Stop on ENTER
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("\r\nUART Trigger: Stopped by user.\n");
            break;
        }

        // Read incoming UART
        while (uartService.available() > 0) {
            char c = uartService.read();

            // echo to terminal so user sees the stream
            terminalView.print(std::string(1, c));

            // sliding buffer
            buf.push_back((uint8_t)c);
            if (buf.size() > MAX_BUF) {
                size_t drop = buf.size() - MAX_BUF;
                buf.erase(buf.begin(), buf.begin() + drop);
            }

            bool matched = false;

            if (!isHex) {
                // text match
                if (buf.size() >= textPattern.size()) {
                    size_t startMin = 0;
                    if (buf.size() > textPattern.size() + 64) {
                        startMin = buf.size() - textPattern.size() - 64;
                    }

                    for (size_t s = startMin; s + textPattern.size() <= buf.size(); ++s) {
                        bool ok = true;
                        for (size_t k = 0; k < textPattern.size(); ++k) {
                            if (buf[s + k] != (uint8_t)textPattern[k]) { ok = false; break; }
                        }
                        if (ok) { matched = true; break; }
                    }
                }
            } else {
                // hex match with ?? wildcards
                if (buf.size() >= hexPattern.size()) {
                    size_t startMin = 0;
                    if (buf.size() > hexPattern.size() + 32) {
                        startMin = buf.size() - hexPattern.size() - 32;
                    }

                    for (size_t s = startMin; s + hexPattern.size() <= buf.size(); ++s) {
                        bool ok = true;
                        for (size_t k = 0; k < hexPattern.size(); ++k) {
                            if (hexMask[k] == 0) continue; // wildcard
                            if (buf[s + k] != hexPattern[k]) { ok = false; break; }
                        }
                        if (ok) { matched = true; break; }
                    }
                }
            }

            if (matched) {
                uint32_t now = millis();
                if (now - lastFire >= COOLDOWN_MS) {
                    lastFire = now;

                    uartService.print(response);
                    terminalView.println("");
                    terminalView.println("\r\n[TRIGGER] match -> sent " + responseRaw);
                    terminalView.println("");

                    // Prevent immediate retrigger
                    buf.clear();
                }
            }
        }
    }
}

/*
Ensure Config
*/
void UartController::ensureConfigured() {
    // hdUartService.end() // It crashed the app for some reasons, not rly needed

    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    // User could have set the same pin to a different usage
    // eg. select UART, then select I2C, then select UART
    // Always reconfigure pins before use
    uartService.end();

    uint8_t rx = state.getUartRxPin();
    uint8_t tx = state.getUartTxPin();
    uint32_t baud = state.getUartBaudRate();
    uint32_t config = state.getUartConfig();
    bool inverted = state.isUartInverted();

    uartService.configure(baud, config, rx, tx, inverted);
}