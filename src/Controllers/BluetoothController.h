#pragma once

#include <string>
#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Services/BluetoothService.h"
#include "Transformers/ArgTransformer.h"
#include "Models/TerminalCommand.h"
#include "Managers/UserInputManager.h"
#include "States/GlobalState.h"
#include "Shells/HelpShell.h"
#include "Shells/MouseShell.h"

class BluetoothController {
public:
    BluetoothController(
        ITerminalView& terminalView,
        IInput& terminalInput,
        IInput& deviceInput,
        BluetoothService& bluetoothService,
        ArgTransformer& argTransformer,
        UserInputManager& userInputManager,
        HelpShell& helpShell,
        MouseShell& mouseShell
    );

    // Entry point for BT command
    void handleCommand(const TerminalCommand& cmd);

    // Ensure BT configuration
    void ensureConfigured();

    // Release BT memory
    void ensureReleased();
    
private:
    ITerminalView& terminalView;
    IInput& terminalInput;
    IInput& deviceInput;
    BluetoothService& bluetoothService;
    ArgTransformer& argTransformer;
    UserInputManager& userInputManager;
    HelpShell& helpShell;
    MouseShell& mouseShell;
    GlobalState& state = GlobalState::getInstance();
    bool configured = false;
    
    // Scan for BT devices
    void handleScan();

    // Pair with BT device
    void handlePair(const TerminalCommand& cmd);

    // Spoof mac addr
    void handleSpoof(const TerminalCommand& cmd);

    // Show BT status
    void handleStatus();

    // Create BT HID server
    void handleServer(const TerminalCommand& cmd);

    // Handle KB action
    void handleKeyboard(const TerminalCommand& cmd);

    // Handle keyboard bridge
    void handleKeyboardBridge();

    // Handle mouse action
    void handleMouse(const TerminalCommand& cmd);

    // Handle mouse jiggle until enter is pressed
    void handleMouseJiggle(const TerminalCommand& cmd);

    // Sniff BT frames
    void handleSniff(const TerminalCommand& cmd);

    // Available BT commands
    void handleHelp();

    // Config BT
    void handleConfig();

    // Reset BT interface
    void handleReset();
};