#pragma once

#include <Interfaces/IInput.h>
#include <Interfaces/IHostSerial.h>
#include <Arduino.h>
#include <vector>

class SerialTerminalInput : public IInput {
public:
    explicit SerialTerminalInput(IHostSerial& hostSerial)
        : hostSerial(hostSerial) {}

    char handler() override;
    void waitPress(uint32_t timeoutMs) override;
    char readChar() override;

private:
    IHostSerial& hostSerial;
};
