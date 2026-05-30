#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "Interfaces/IInput.h"

// Exclusive one-shot USB CDC adapter for avrdude -c buspirate.
// Emulates the Bus Pirate legacy binary raw SPI mode used for AVR ISP.

struct AvrDudeBusPirateConfig {
    uint8_t csPin;
    uint8_t sckPin;
    uint8_t misoPin;
    uint8_t mosiPin;
    uint32_t frequency;
};

class AvrDudeBusPirateAdapter {
public:
    static void run(const AvrDudeBusPirateConfig& config, IInput& input);

private:
    enum class ProtocolState : uint8_t {
        WaitBbio,
        RawBitbang,
        RawSpi,
    };

    static inline AvrDudeBusPirateConfig config = {0, 0, 0, 0, 1000000};
    static inline SPIClass spi{HSPI};
    static inline ProtocolState state = ProtocolState::WaitBbio;
    static inline uint8_t zeroCount = 0;
    static inline uint8_t spiMode = SPI_MODE0;
    static inline bool reqExtendedHighByteWrite = false;

    static void initializeSpi();
    static void setChipSelect(bool asserted);
    static void handleByte(uint8_t command, IInput& input);
    static void handleRawBitbangCommand(uint8_t command);
    static void handleRawSpiCommand(uint8_t command, IInput& input);
    static void handleBulkTransfer(uint8_t command, IInput& input);
    static void handleWriteThenRead(bool autoChipSelect, IInput& input);
    static void handleAvrExtendedCommand(IInput& input);
    static void handleAvrExtendedPagedRead(IInput& input);
    static void configurePeripherals(uint8_t command);
    static void configureSpeed(uint8_t command);
    static void configureSpi(uint8_t command);
    static uint8_t transfer(uint8_t value);
    static uint8_t readByte(IInput& input);
    static uint16_t readBe16(IInput& input);
    static uint32_t readBe32(IInput& input);
    static void writeByte(uint8_t value);
    static void writeBytes(const uint8_t* data, size_t length);
    static void writeString(const char* value);
    static bool inputRequestedReset(IInput& input);
    static void updateExtendedHighByteRequirement(const uint8_t* tx, const uint8_t* rx, uint16_t length);
};
