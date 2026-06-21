#pragma once

#include <string>
#include <vector>
#include "Models/TerminalCommand.h"
#include "Models/ByteCode.h"
#include "Transformers/InstructionTransformer.h"
#include "Enums/ModeEnum.h"
#include "Providers/DependencyProvider.h"
#include "Enums/ByteCodeEnum.h"
#include "Enums/TerminalTypeEnum.h"
#include "Interfaces/ITerminalView.h"

class ActionDispatcher {
public:
    // Constructor with dependency injection
    explicit ActionDispatcher(DependencyProvider& provider);

    // Initialize
    void setup(TerminalTypeEnum terminalType, std::string terminaInfos);

    // Main loop that handles user input
    void run();

    // Process raw user action
    void dispatch(const std::string& raw);

private:
    size_t MAX_ALLOWED_COMMAND_LENGTH = 512;
    DependencyProvider& provider;
    GlobalState& state = GlobalState::getInstance();
    bool recoveryButtonDown = false;
    bool recoveryClickPending = false;
    uint32_t recoveryFirstClickMs = 0;


    // Handle a command
    void dispatchCommand(const TerminalCommand& cmd);

    // dipatch repeat command
    void dispatchRepeatCommands(const std::string& raw);

    // dispatch a sequence of commands (pipeline)
    void dispatchPipelineCommands(const std::string& raw);

    // Handle a sequence of bytecode instructions
    void dispatchInstructions(const std::vector<Instruction>& instructions);

    // Switch to a different mode
    void setCurrentMode(ModeEnum newMode);

    // Release mode resources if needed
    void releaseMode(ModeEnum currentMode, ModeEnum newMode);

    // Read user input with cursor support
    std::string getUserAction();

    // Detect double-click of the board/user button while Web UI mode is active.
    void handleDeviceRecoveryGesture();

    // Handle ANSI escape sequences (arrows, etc.)
    bool handleEscapeSequence(char c, std::string& inputLine, size_t& cursorIndex, const std::string& mode);

    // Handle Tab key for autocompletion
    bool handleTabCompletion(char c, std::string& inputLine, size_t& cursorIndex, const std::string& mode);

    // Handle Cardputer arrows
    bool handleCardputerEscapeSequence(char c, size_t& cursorIndex, std::string& inputLine, const std::string& mode);

    // Handle backspace logic
    bool handleBackspace(char c, std::string& inputLine, size_t& cursorIndex, const std::string& mode);

    // Handle printable characters insertion
    bool handlePrintableChar(char c, std::string& inputLine, size_t& cursorIndex, const std::string& mode);

    // Handle Enter key and dispatch line
    bool handleEnterKey(char c, const std::string& inputLine);
};
