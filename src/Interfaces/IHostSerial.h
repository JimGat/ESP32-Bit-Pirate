#pragma once

#include <Arduino.h>

class IHostSerial {
public:
    virtual ~IHostSerial() = default;

    virtual void begin(unsigned long baud) = 0;
    virtual void waitReady() = 0;

    virtual int available() = 0;
    virtual int availableForWrite() = 0;
    virtual int peek() = 0;
    virtual int read() = 0;
    virtual size_t readBytes(uint8_t* buffer, size_t length) = 0;

    virtual size_t write(uint8_t value) = 0;
    virtual size_t write(const uint8_t* buffer, size_t length) = 0;
    virtual size_t print(const char* text) = 0;
    virtual size_t println(const char* text) = 0;
    virtual size_t println() = 0;

    virtual void flush() = 0;
    virtual bool setRxBufferSize(size_t size) = 0;
    virtual void setTimeout(unsigned long timeoutMs) = 0;
    virtual uint32_t baudRate() = 0;
    virtual void disableReboot() = 0;
    virtual void waitForPress() = 0;
};
