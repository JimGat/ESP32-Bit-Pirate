#pragma once

#include <Arduino.h>
#include "Interfaces/IInput.h"
#include "Interfaces/IHostSerial.h"

// Exclusive USB CDC <-> UART bridge

struct UsbUartBridgeConfig {
    uint8_t uartRxPin;
    uint8_t uartTxPin;
    bool uartInverted;
};

class UsbUartBridgeAdapter {
public:
    static void run(const UsbUartBridgeConfig& config, IInput& input, IHostSerial& hostSerial);

private:
    static inline UsbUartBridgeConfig bridgeConfig = {0, 0, false};
    static inline IHostSerial* hostSerial = nullptr;
    static inline unsigned long currentBaudRate = 0;
    static inline uint32_t currentUartConfig = SERIAL_8N1;

    static uint32_t cdcLineCodingToUartConfig(uint8_t dataBits, uint8_t parity, uint8_t stopBits);
    static void configureUart(unsigned long baudRate, uint32_t uartConfig);

    static void onLineCoding(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);

    static void pumpUartToUsb();
    static void pumpUsbToUart();
    static void pumpBridgeOnce();
};