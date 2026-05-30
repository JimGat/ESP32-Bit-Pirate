#include "UsbS3Controller.h"

/*
Constructor
*/
UsbS3Controller::UsbS3Controller(
    ITerminalView& terminalView,
    IInput& terminalInput,
    IInput& deviceInput,
    UsbS3Service& usbService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    HelpShell& helpShell,
    UsbAdapterShell& usbAdapterShell
)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      deviceInput(deviceInput),
      usbService(usbService),
      argTransformer(argTransformer),
      userInputManager(userInputManager),
      helpShell(helpShell),
      usbAdapterShell(usbAdapterShell)
{}

/*
Entry point for command
*/
void UsbS3Controller::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "stick" || cmd.getRoot() == "storage") handleUsbStick();
    else if (cmd.getRoot() == "keyboard") handleKeyboard(cmd);
    else if (cmd.getRoot() == "mouse") handleMouse(cmd);
    else if (cmd.getRoot() == "gamepad") handleGamepad(cmd);
    else if (cmd.getRoot() == "sysctrl") handleSysCtrl(cmd);
    else if (cmd.getRoot() == "host") handleHost();
    else if (cmd.getRoot() == "adapter" || cmd.getRoot() == "adapters") handleAdapters();
    else if (cmd.getRoot() == "reset") handleReset();
    else if (cmd.getRoot() == "config") handleConfig();
    else handleHelp();
}

/*
Keyboard
*/
void UsbS3Controller::handleKeyboard(const TerminalCommand& cmd) {
    auto sub = cmd.getSubcommand();

    usbService.configure(state.getUSBProductString(), state.getUSBManufacturerString(), state.getUSBSerialString(), state.getUSBVid(), state.getUSBPid(), state.getWebUSBString());
    if (sub.empty()) handleKeyboardBridge();
    else if (sub == "bridge") handleKeyboardBridge();
    else handleKeyboardSend(cmd);
}

/*
Keyboard Send
*/
void UsbS3Controller::handleKeyboardSend(const TerminalCommand& cmd) {
    terminalView.println("USB Keyboard: Configuring...");
    usbService.keyboardBegin();
    terminalView.println("USB Keyboard: Initialize...");
    auto full = cmd.getArgs().empty() ? cmd.getSubcommand() : cmd.getSubcommand() + " " + cmd.getArgs();
    auto decoded = argTransformer.decodeEscapes(full);
    usbService.keyboardSendString(decoded);
    // usbService.reset();
    terminalView.println("USB Keyboard: String sent.");
}

/*
Keyboard Bridge
*/
void UsbS3Controller::handleKeyboardBridge() {
    terminalView.println("USB Keyboard Bridge: Sending all keys to USB HID.");
    usbService.keyboardBegin();

    auto sameHost = false;
    if (state.getTerminalMode() != TerminalTypeEnum::Standalone) {
        terminalView.println("\n [⚠️  WARNING] ");
        terminalView.println(" If the USB device is plugged on the same host as");
        terminalView.println(" the terminal, it may cause looping issues with ENTER.");
        terminalView.println(" (That makes no sense to bridge keyboard on same host)\n");
        
        sameHost = userInputManager.readYesNo("Are you connected on the same host? (y/n)", true);
    
        if (sameHost) {
            terminalView.println("Same host, ENTER key will not be sent to USB HID.");
        }
    }    

    terminalView.println("USB Keyboard: Bridge started.. Press [ANY ESP32 BUTTON] to stop.");

    while (true) {
        // Esp32 button to stop
        char k = deviceInput.readChar();
        if (k != KEY_NONE) {
            terminalView.println("\r\nUSB Keyboard Bridge: Stopped by user.");
            break;
        }
        
        // Terminal to keyboard hid
        char c = terminalInput.readChar();

        // if we send \n in the terminal web browser
        // and the usb hid are on the same host
        // then it will loop infinitely
        if (c != KEY_NONE) { 
            if (c == '\n' && sameHost) continue;
            usbService.keyboardSendString(std::string(1, c));
            delay(20); // slow down looping
        }
    }
}

/*
Mouse Move
*/
void UsbS3Controller::handleMouseMove(const TerminalCommand& cmd) {
    int x, y = 0;

    // mouse move x y
    if (cmd.getSubcommand() == "move") {
        auto args = argTransformer.splitArgs(cmd.getArgs());
        if (args.size() < 2 ||
            argTransformer.isValidSignedNumber(args[0]) == false ||
            argTransformer.isValidSignedNumber(args[1]) == false) {
            terminalView.println("Usage: mouse move <x> <y>");
            return;
        }
        x = argTransformer.toClampedInt8(args[0]);
        y = argTransformer.toClampedInt8(args[1]);
    // mouse x y
    } else {
        if (argTransformer.isValidSignedNumber(cmd.getSubcommand()) == false ||
            argTransformer.isValidSignedNumber(cmd.getArgs()) == false) {
            terminalView.println("Usage: mouse <x> <y>");
            return;
        }
        x = argTransformer.toClampedInt8(cmd.getSubcommand());
        y = argTransformer.toClampedInt8(cmd.getArgs());
    }

    usbService.mouseMove(x, y);
    terminalView.println("USB Mouse: Moved by (" + std::to_string(x) + ", " + std::to_string(y) + ")");
}

/*
Mouse Click
*/
void UsbS3Controller::handleMouseClick() {
    // Left click
    usbService.mouseClick(1);
    delay(100);
    usbService.mouseRelease(1);
    terminalView.println("USB Mouse: Click sent.");
}

/*
Mouse
*/
void UsbS3Controller::handleMouse(const TerminalCommand& cmd)  {
    // Configure descriptors
    usbService.configure(
        state.getUSBProductString(),
        state.getUSBManufacturerString(),
        state.getUSBSerialString(),
        state.getUSBVid(),
        state.getUSBPid(),
        state.getWebUSBString()
    );

    terminalView.println("USB Mouse: Configuring HID...");
    usbService.mouseBegin();
    terminalView.println("USB Mouse: Initialize HID...");

    // Interactive menu if no subcommand
    if (cmd.getSubcommand().empty()) {
        std::vector<std::string> actions = {
            " ↑ Move up",
            " ↓ Move down",
            " ← Move left",
            " → Move right",
            " ◀ Left click",
            " ▶ Right click",
            " ⚙ Configure move",
            " ⏹ Exit"
        };

        
        bool loop = true;
        int lastIndex = actions.size() - 1; // keeps selection between inputs
        uint8_t step = 50; // default step for moves
        while (loop) {
            terminalView.println("\n=== USB Mouse ===");
            int choice = userInputManager.readValidatedChoiceIndex("Select action", actions, lastIndex);

            // keep last index only for real actions (not exit)
            if (choice >= 0 && choice <= 5) lastIndex = choice;

            switch (choice) {
                case 0: usbService.mouseMove(0, step * -1); terminalView.println("\nUSB Mouse: Up."); break;
                case 1: usbService.mouseMove(0,  step); terminalView.println("\nUSB Mouse: Down."); break;
                case 2: usbService.mouseMove(step * -1, 0); terminalView.println("\nUSB Mouse: Left."); break;
                case 3: usbService.mouseMove(step, 0); terminalView.println("\nUSB Mouse: Right."); break;

                case 4:
                    usbService.mouseClick(1);
                    terminalView.println("\nUSB Mouse: Left click.");
                    break;

                case 5:
                    usbService.mouseClick(2);
                    terminalView.println("\nUSB Mouse: Right click.");
                    break;

                case 6:
                    step = userInputManager.readValidatedUint8("Configure step for moves", step, 1, 127);
                    terminalView.println("\nUSB Mouse: Step configured to " + std::to_string(step) + ".");
                    break;

                default:
                    loop = false;
                    break;
            }
        }

        terminalView.println("Exiting USB Mouse...\n");
        return;
    }

    // Existing non-interactive usage
    if (cmd.getSubcommand() == "click") {
        handleMouseClick();
    }
    else if (cmd.getSubcommand() == "jiggle") {
        handleMouseJiggle(cmd);
    }
    else {
        handleMouseMove(cmd); // supports "mouse move x y" and "mouse x y"
    }
}

/*
Mouse Jiggle
*/
void UsbS3Controller::handleMouseJiggle(const TerminalCommand& cmd) {
    int intervalMs = 1000; // defaut

    if (!cmd.getArgs().empty() && argTransformer.isValidNumber(cmd.getArgs())) {
        auto intervalMs = argTransformer.parseHexOrDec32(cmd.getArgs());
    }

    terminalView.println("USB Mouse: Jiggle started (" + std::to_string(intervalMs) + " ms)... Press [ENTER] to stop.");

    while (true) {
        // Random moves
        int dx = (int)random(-127, 127);
        int dy = (int)random(-127, 127);
        if (dx == 0 && dy == 0) dx = 1;

        usbService.mouseMove(dx, dy);

        // wait interval while listening for ENTER
        unsigned long t0 = millis();
        while ((millis() - t0) < intervalMs) {
            auto c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("USB Mouse: Jiggle stopped.\n");
                return;
            }
            delay(10);
        }
    }
}

/*
Gamepad
*/
void UsbS3Controller::handleGamepad(const TerminalCommand& cmd) {
    terminalView.println("USB Gamepad: Configuring HID...");

    usbService.configure(
        state.getUSBProductString(),
        state.getUSBManufacturerString(),
        state.getUSBSerialString(),
        state.getUSBVid(),
        state.getUSBPid(),
        state.getWebUSBString()
    );

    usbService.gamepadBegin();

    std::string sub = cmd.getSubcommand();
    std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);

    // Interactive menu
    if (sub.empty()) {
        std::vector<std::string> actions = {
            " ⬆ Up",
            " ⬇ Down",
            " ⬅ Left",
            " ➡ Right",
            " Ⓐ A",
            " Ⓑ B",
            " ▶ Start",
            " ◀ Select",
            " ⏹ Exit"
        };

        bool loop = true;
        uint8_t lastButton = actions.size() - 1;
        while (loop) {
            terminalView.println("\n=== USB Gamepad ===");
            int choice = userInputManager.readValidatedChoiceIndex("Select action", actions, lastButton);
            
            switch (choice) {
                case 0: usbService.gamepadPress("up");     terminalView.println("\nUSB Gamepad: Up sent."); break;
                case 1: usbService.gamepadPress("down");   terminalView.println("\nUSB Gamepad: Down sent."); break;
                case 2: usbService.gamepadPress("left");   terminalView.println("\nUSB Gamepad: Left sent."); break;
                case 3: usbService.gamepadPress("right");  terminalView.println("\nUSB Gamepad: Right sent."); break;
                case 4: usbService.gamepadPress("a");      terminalView.println("\nUSB Gamepad: A sent."); break;
                case 5: usbService.gamepadPress("b");      terminalView.println("\nUSB Gamepad: B sent."); break;
                case 6: usbService.gamepadPress("start");  terminalView.println("\nUSB Gamepad: Start sent."); break;
                case 7: usbService.gamepadPress("select"); terminalView.println("\nUSB Gamepad: Select sent."); break;
                case 8:
                default:
                    loop = false;
                    break;
            }
            lastButton = choice;

        }

        terminalView.println("Exiting USB Gamepad...\n");
        return;
    }

    // Non-interactive
    if (sub == "up" || sub == "down" || sub == "left" || sub == "right" ||
        sub == "a" || sub == "b" || sub == "start" || sub == "select") {
        usbService.gamepadPress(sub);
        terminalView.println("USB Gamepad: Key sent.");
        return;
    }

    terminalView.println("Usage: gamepad");
    terminalView.println("       gamepad up|down|left|right|a|b");
    terminalView.println("       gamepad start|select");
}

/*
SysCtrl
*/
void UsbS3Controller::handleSysCtrl(const TerminalCommand& cmd) {

    // Configure descriptors
    usbService.configure(
        state.getUSBProductString(),
        state.getUSBManufacturerString(),
        state.getUSBSerialString(),
        state.getUSBVid(),
        state.getUSBPid(),
        state.getWebUSBString()
    );

    // Get subcommand and lowercase it
    std::string sub = cmd.getSubcommand();
    std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);

    // Ensure SystemControl interface is ready
    usbService.systemControlBegin();

    // If no subcommand, interactive menu
    if (sub.empty()) {
        std::vector<std::string> actions = {
            " 💤 Sleep (Standby)",
            " 🔔 Wake host",
            " 🔌 Power off",
            " 🚪 Exit"
        };

        bool loop = true;
        while (loop) {
            terminalView.println("\n=== USB System Control ===");
            int choice = userInputManager.readValidatedChoiceIndex("Select action", actions, actions.size() - 1);

            switch (choice) {
                case 0:
                    terminalView.println("\nUSB System: Sending SLEEP...");
                    usbService.systemSleep();
                    terminalView.println("USB System: Done.");
                    break;

                case 1:
                    terminalView.println("\nUSB System: Sending WAKE...");
                    usbService.systemWake();
                    terminalView.println("USB System: Done.");
                    break;

                case 2:
                    terminalView.println("\nUSB System: Sending POWER OFF...");
                    terminalView.println("\n [⚠️  WARNING] ");
                    terminalView.println(" OS may ignore it or ask confirmation.\n");

                    usbService.systemPowerOff();
                    terminalView.println("USB System: Done.");
                    break;

                case 3: // Exit
                default:
                    loop = false;
                    break;
            }
        }

        terminalView.println("Exiting USB System Control...\n");
        return;
    }

    // Non interactive mode
    if (sub == "sleep" || sub == "standby" || sub == "suspend") {
        terminalView.println("USB System: Sending SLEEP...");
        usbService.systemSleep();
        terminalView.println("USB System: Done.");
    }
    else if (sub == "wake" || sub == "wakeup" || sub == "resume") {
        terminalView.println("USB System: Sending WAKE...");
        usbService.systemWake();
        terminalView.println("USB System: Done.");
    }
    else if (sub == "off" || sub == "poweroff" || sub == "power off") {
        terminalView.println("\nUSB System: Sending POWER OFF...");
        terminalView.println("\n [⚠️  WARNING] ");
        terminalView.println(" OS may ignore it or ask confirmation.\n");
        usbService.systemPowerOff();
        terminalView.println("USB System: Done.");
    }
    else {
        terminalView.println("Usage: sysctrl");
        terminalView.println("       sysctrl sleep");
        terminalView.println("       sysctrl wake");
        terminalView.println("       sysctrl off");
    }
}

/*
Stick
*/
void UsbS3Controller::handleUsbStick() {
    terminalView.println("USB Stick: Starting... USB Drive can take 30sec to appear");
    usbService.storageBegin(state.getSdCardCsPin(), state.getSdCardClkPin(),
                            state.getSdCardMisoPin(), state.getSdCardMosiPin());

    if (usbService.isStorageActive()) {
        terminalView.println("\n ✅ USB Stick configured. Mounting drive... (Can take up to 30 sec)\n");
    } else {
        terminalView.println("\n ❌ USB Stick configuration failed. No SD card detected.\n");
    }
}

/*
Config
*/
void UsbS3Controller::handleConfig() {
    terminalView.println("USB Configuration:");

    auto confirm = userInputManager.readYesNo("Configure SD card GPIOs for USB?", false);

    if (confirm) {
        auto forbidden = state.getProtectedPins();
    
        uint8_t cs = userInputManager.readValidatedPinNumber("SD Card CS GPIO", state.getSdCardCsPin(), forbidden);
        state.setSdCardCsPin(cs);
        forbidden.push_back(cs);
    
        uint8_t clk = userInputManager.readValidatedPinNumber("SD Card CLK GPIO", state.getSdCardClkPin(), forbidden);
        state.setSdCardClkPin(clk);
        forbidden.push_back(clk);
    
        uint8_t miso = userInputManager.readValidatedPinNumber("SD Card MISO GPIO", state.getSdCardMisoPin(), forbidden);
        state.setSdCardMisoPin(miso);
        forbidden.push_back(miso);
    
        uint8_t mosi = userInputManager.readValidatedPinNumber("SD Card MOSI GPIO", state.getSdCardMosiPin(), forbidden);
        state.setSdCardMosiPin(mosi);
        forbidden.push_back(mosi);
    }

    std::string productString = userInputManager.readSanitizedString("USB Product String", state.getUSBProductString(), false);
    state.setUSBProductString(productString);

    std::string manufacturerString = userInputManager.readSanitizedString("USB Manufacturer String", state.getUSBManufacturerString(), false);
    state.setUSBManufacturerString(manufacturerString);

    auto serialStr = state.getUSBSerialString().empty() ? usbService.getUsbSerialFromEfuseMac() : state.getUSBSerialString();
    std::string serialString = userInputManager.readSanitizedString("USB Serial String", serialStr, false);
    state.setUSBSerialString(serialString);

    uint16_t vid = userInputManager.readValidatedUint16("USB VID", state.getUSBVid(), true);
    state.setUSBVid(vid);

    uint16_t pid = userInputManager.readValidatedUint16("USB PID", state.getUSBPid(), true);
    state.setUSBPid(pid);

    terminalView.println("USB Configured.");

    if (state.getTerminalMode() == TerminalTypeEnum::Standalone) {
        terminalView.println("");
        return;
    };

    terminalView.println(""); // align
}

/*
Host
*/
void UsbS3Controller::handleHost() {
    if (!usbService.isHostActive()) {
        terminalView.println("USB Host: Once started, CDC/HID/MSC will be unavailable until restart.");

        if (state.getTerminalMode() == TerminalTypeEnum::SerialPort) {
            terminalView.println("\n [⚠️  WARNING] ");
            terminalView.println(" Active USB Serial interface will be stopped.");
            terminalView.println(" Use this feature with the WiFi web terminal.");
        }

        auto confirm = userInputManager.readYesNo("\nSwitch to host to connect USB Devices to the ESP32?", false);
        if (!confirm) {
            terminalView.println("USB Host: Action cancelled by user.\n");
            return;
        }
    }

    if (!usbService.usbHostBegin()) {
        terminalView.println("USB Host: Failed to start.\n");
        return;
    }

    terminalView.println("");
    terminalView.println("USB Host: Plug a USB device to the ESP32... Press [ENTER] to stop.\n");
    terminalView.println(" [ℹ️  INFORMATION] ");
    terminalView.println(" USB Host will print device descriptors.");
    terminalView.println(" This feature works only with USB *devices*,");
    terminalView.println(" such as keyboards, mice, gamepads, etc.");
    terminalView.println(" Make sure your board provides 5V on USB.\n");
    while (true) {
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') break;

        std::string msg = usbService.usbHostTick();
        if (!msg.empty()) {
            terminalView.println(msg);
        }

    }

    // TODO: usbService.usbHostEnd(); can't be called because it will stop the host 
    // and make the USB interface instable, find a way to switch back to default mode
    terminalView.println("\nUSB Host: Stopped by user.\n");
}

/* Adapters */
void UsbS3Controller::handleAdapters() {
    usbAdapterShell.run();
}

/*
Reset
*/
void UsbS3Controller::handleReset() {
    usbService.reset();
    terminalView.println("USB Reset: Disable interfaces...");
}

/*
Help
*/
void UsbS3Controller::handleHelp() {
    terminalView.println("\nUnknown command. Available USB commands:");
    helpShell.run(state.getCurrentMode(), false);
}

/*
Ensure Configuration
*/
void UsbS3Controller::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
    }
}
