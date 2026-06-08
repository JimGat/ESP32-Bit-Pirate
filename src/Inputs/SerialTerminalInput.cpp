#include "SerialTerminalInput.h"
#include <Arduino.h>

char SerialTerminalInput::handler() {
    while (!hostSerial.available()) {}
    return static_cast<char>(hostSerial.read());
}

void SerialTerminalInput::waitPress(uint32_t timeoutMs) {
    (void)timeoutMs; // currently not used
    hostSerial.waitForPress();
    hostSerial.read(); // discard
}

char SerialTerminalInput::readChar() {
    if (hostSerial.available()) {
        return static_cast<char>(hostSerial.read());
    }
    return KEY_NONE;
}