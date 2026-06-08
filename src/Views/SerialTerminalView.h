#pragma once

#include <Arduino.h>
#include <string>
#include <Interfaces/IHostSerial.h>
#include <Interfaces/ITerminalView.h>
#include <States/GlobalState.h>
#include <Enums/TerminalTypeEnum.h>

class SerialTerminalView : public ITerminalView {
public:
    explicit SerialTerminalView(IHostSerial& hostSerial)
        : hostSerial(hostSerial) {}

    void initialize() override;
    void welcome(TerminalTypeEnum& terminalType, std::string& terminalInfos) override;
    void print(const std::string& text) override;
    void print(const uint8_t data) override;
    void println(const std::string& text) override;
    void printPrompt(const std::string& mode = "HIZ") override;
    void clear() override;
    void waitPress() override;
    void setBaudrate(unsigned long baudrate);
    
private:
    IHostSerial& hostSerial;
    unsigned long baudrate = 1152200;
};
