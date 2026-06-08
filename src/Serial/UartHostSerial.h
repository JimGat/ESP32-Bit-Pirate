#pragma once

#include <Serial/DefaultHostSerial.h>

class UartHostSerial : public DefaultHostSerial {
public:
    void waitReady() override {
        // UART host link does not require USB CDC attach wait
    }

    void disableReboot() override {
        // UART host link does not use USB CDC reboot behavior
    }
};
