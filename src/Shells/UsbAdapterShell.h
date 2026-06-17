#pragma once

#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Managers/UserInputManager.h"
#include "Services/NvsService.h"
#include "States/GlobalState.h"

class UsbAdapterShell {
public:
    UsbAdapterShell(ITerminalView& tv,
                    IInput& in,
                    UserInputManager& uim,
                    NvsService& nvs);

    void run();
    void rebootUsbUartBridge();
    void rebootFlashromSerprog();
    void rebootAvrDudeBusPirate();
    void rebootBpio2();
    void rebootSumpLogicAnalyzer();
    void rebootOpenOcdBusPirate();
    void rebootInfraredToy();
    void rebootSubGhzRawCdc();

private:
    void rebootIntoAdapter(const char* title,
                           const char* description,
                           const char* example,
                           const char* returnInstruction = "Reset or press a device button to return to normal mode.");

    ITerminalView& terminalView;
    IInput& terminalInput;
    UserInputManager& userInputManager;
    NvsService& nvsService;
    GlobalState& state = GlobalState::getInstance();

    inline static constexpr const char* actions[] = {
        " USB-UART bridge",
        " Flashrom serprog",
        " AVRDUDE Bus Pirate SPI",
        " SUMP logic analyzer",
        " OpenOCD JTAG/SWD",
        " USB IR Toy / LIRC",
        " SubGHz CDC CC1101",
        " Bit Bang IO/SPI/I2C",
        " Exit"
    };
    inline static constexpr size_t actionsCount = sizeof(actions) / sizeof(actions[0]);
};
