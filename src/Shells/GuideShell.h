#pragma once
#include <vector>
#include <string>

#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Managers/UserInputManager.h"

class GuideShell {
public:
    GuideShell(ITerminalView& tv,
               IInput& in,
               UserInputManager& uim);

    void run();

private:
    ITerminalView& terminalView;
    IInput& terminalInput;
    UserInputManager& userInputManager;

    inline static constexpr const char* actions[] = {
        " 🚀 Quick start",
        " 🧩 Command examples",
        " 🌐 Web terminal",
        " 🧰 Instruction syntax",
        " 🐍 Python automation",
        " 📒 File System",
        " ☠️  Pirate Assistant",
        " ⚠️  Safety (voltage)",
        " 🚪 Exit"
    };

    inline static constexpr size_t actionsCount =
        sizeof(actions) / sizeof(actions[0]);

    void cmdQuickStart();
    void cmdExamples();
    void cmdWebTerminal();
    void cmdInstructionSyntax();
    void cmdPythonAutomation();
    void cmdLittleFS();
    void cmdPirateAssistant();
    void cmdSafety();
};
