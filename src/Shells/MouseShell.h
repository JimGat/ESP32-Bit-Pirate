#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Managers/UserInputManager.h"

class MouseShell {
public:
    using MoveFn = std::function<void(int, int)>;
    using ClickFn = std::function<void()>;

    MouseShell(ITerminalView& terminalView,
               IInput& terminalInput,
               UserInputManager& userInputManager);

    void run(const std::string& title,
             const MoveFn& move,
             const ClickFn& leftClick,
             const ClickFn& rightClick);

private:
    inline static constexpr const char* actions[] = {
        " ↑ Move up",
        " ↓ Move down",
        " ← Move left",
        " → Move right",
        " ◀ Left click",
        " ▶ Right click",
        " ⚙ Configure move",
        " ↺ Jiggle",
        " ⏹ Exit"
    };
    static constexpr size_t actionsCount = sizeof(actions) / sizeof(actions[0]);

    void runJiggle(const std::string& title, const MoveFn& move);

    ITerminalView& terminalView;
    IInput& terminalInput;
    UserInputManager& userInputManager;
};