#include <Arduino.h>

#include "MouseShell.h"

MouseShell::MouseShell(ITerminalView& terminalView,
                       IInput& terminalInput,
                       UserInputManager& userInputManager)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      userInputManager(userInputManager) {}

void MouseShell::run(const std::string& title,
                     const MoveFn& move,
                     const ClickFn& leftClick,
                     const ClickFn& rightClick) {
    bool loop = true;
    int lastIndex = static_cast<int>(actionsCount - 1);
    uint8_t step = 50;

    while (loop) {
        terminalView.println("\n=== " + title + " ===");
        std::vector<std::string> actionsList(actions, actions + actionsCount);
        int choice = userInputManager.readValidatedChoiceIndex("Select action", actionsList, lastIndex);

        if (choice >= 0 && choice <= 5) {
            lastIndex = choice;
        }

        switch (choice) {
            case 0: move(0, -static_cast<int>(step)); terminalView.println("\n" + title + ": Up."); break;
            case 1: move(0, static_cast<int>(step)); terminalView.println("\n" + title + ": Down."); break;
            case 2: move(-static_cast<int>(step), 0); terminalView.println("\n" + title + ": Left."); break;
            case 3: move(static_cast<int>(step), 0); terminalView.println("\n" + title + ": Right."); break;
            case 4: leftClick(); terminalView.println("\n" + title + ": Left click."); break;
            case 5: rightClick(); terminalView.println("\n" + title + ": Right click."); break;
            case 6:
                step = userInputManager.readValidatedUint8("Configure step for moves", step, 1, 127);
                terminalView.println("\n" + title + ": Step configured to " + std::to_string(step) + ".");
                break;
            case 7:
                runJiggle(title, move);
                break;
            default:
                loop = false;
                break;
        }
    }

    terminalView.println("Exiting " + title + "...\n");
}

void MouseShell::runJiggle(const std::string& title, const MoveFn& move) {
    constexpr int intervalMs = 1000;

    terminalView.println(title + ": Jiggle started (" + std::to_string(intervalMs) + " ms)... Press [ENTER] to stop.");

    while (true) {
        int dx = (int)random(-127, 127);
        int dy = (int)random(-127, 127);
        if (dx == 0 && dy == 0) {
            dx = 1;
        }

        move(dx, dy);

        unsigned long t0 = millis();
        while ((millis() - t0) < static_cast<unsigned long>(intervalMs)) {
            auto c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println(title + ": Jiggle stopped.\n");
                return;
            }
            delay(10);
        }
    }
}