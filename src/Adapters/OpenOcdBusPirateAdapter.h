#pragma once

#include <Arduino.h>
#include "Interfaces/IInput.h"
#include "Interfaces/IHostSerial.h"

// Exclusive one-shot USB CDC adapter for OpenOCD's Bus Pirate transport.
// Provides the legacy BBIO entry points plus minimal JTAG and SWD raw-wire modes.

struct OpenOcdBusPirateConfig {
    uint8_t tckPin;
    uint8_t tmsPin;
    uint8_t tdiPin;
    uint8_t tdoPin;
    uint8_t swclkPin;
    uint8_t swdioPin;
};

class OpenOcdBusPirateAdapter {
public:
    static void run(const OpenOcdBusPirateConfig& config, IInput& input, IHostSerial& hostSerial);

private:
    static void configurePins();
    static void runBitbangMode(IInput& input);
    static void runJtagMode(IInput& input);
    static void runSwdRawWireMode(IInput& input);

    static uint8_t readByte(IInput& input);
    static void writeByte(uint8_t value);
    static void writeBytes(const uint8_t* data, size_t length);
    static void writeString(const char* value);
    static bool inputRequestedReset(IInput& input);

    static uint8_t readJtagTdo();
    static void writeJtagTdi(bool value);
    static void writeJtagTms(bool value);
    static void shiftJtagTap(uint16_t bitCount, IInput& input);
    static uint8_t swdWriteByte(uint8_t value, bool mayBeSwdRequest);
    static bool isSwdRequestByte(uint8_t value);
    static void swdClockLow();
    static void swdClockHigh();
    static void swdDataLow();
    static void swdDataHigh();
    static void swdReleaseData();
    static void swdClockPulse();
    static void swdClockTick();
    static void swdWriteBit(uint8_t value);
    static uint8_t swdReadBit();
    static uint8_t swdReadBitClocked();
    static uint8_t swdReadByte();
    static uint8_t swdWriteByte(uint8_t value);
    static uint8_t reverseByte(uint8_t value);

    static uint8_t applyBitbangPinDirections(uint8_t command);
    static uint8_t applyBitbangPinLevels(uint8_t command);
    static uint8_t readBitbangPins();

    static inline OpenOcdBusPirateConfig config = {0, 0, 0, 0, 0, 0};
    static inline IHostSerial* hostSerial = nullptr;
    static inline bool swdLsbFirst = true;
    static inline bool swdIdleClockHigh = false;
    static inline bool swdNeedsTurnaround = false;
};
