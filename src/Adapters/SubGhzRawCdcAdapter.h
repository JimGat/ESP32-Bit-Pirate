#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <string>
#include <vector>
#include "Interfaces/IInput.h"
#include "Interfaces/IHostSerial.h"
#include "Services/SubGhzService.h"

// Exclusive one-shot USB CDC adapter exposing CC1101 RAW OOK

struct SubGhzRawCdcConfig {
    uint8_t sckPin;
    uint8_t misoPin;
    uint8_t mosiPin;
    uint8_t csPin;
    uint8_t gdo0Pin;
    float frequencyMhz;
    int8_t paDbm;
    uint32_t baudrate;
};

class SubGhzRawCdcAdapter {
public:
    static void run(const SubGhzRawCdcConfig& config, IInput& input, IHostSerial& hostSerial);

private:
    static constexpr uint32_t DEFAULT_BAUDRATE = 38400;
    static constexpr uint32_t INPUT_POLL_INTERVAL_MS = 25;
    static constexpr uint32_t RSSI_HOLD_MS = 20;
    static constexpr size_t MAX_COMMAND_LENGTH = 32 * 1024;
    static constexpr size_t MAX_TX_TIMINGS = 4096;
    static constexpr bool RAW_RX_INVERT_LEVEL = true;
    static constexpr bool RAW_TX_INVERT_POLARITY = true;

    static void begin();
    static bool allocateLazyObjects();
    static void pollUsb();
    static void handleLine(const std::string& line);
    static void handleFrequencyCommand(const std::string& value);
    static void handlePresetCommand(const std::string& value);
    static void handleRxModeCommand(const std::string& value);
    static void handleRawSendCommand(const std::string& value);
    static void pollRawRx();
    static void startRxReporting();
    static void stopRxReporting();
    static void restartRxReporting();
    static void printHelp();
    static void printVersion();
    static void printOk();
    static void printError(const char* message);
    static bool printRawFrame(const std::vector<rmt_symbol_word_t>& frame);
    static bool parseTimings(const std::string& input, std::vector<int32_t>& timings);
    static void normalizeTxPolarity(std::vector<int32_t>& timings);
    static bool parseFrequency(const std::string& input, float& mhz);
    static std::string trim(const std::string& input);
    static std::string stripRawPrefix(const std::string& input);

    static inline SubGhzRawCdcConfig* runtimeConfig = nullptr;
    static inline IHostSerial* hostSerial = nullptr;
    static inline IInput* input = nullptr;
    static inline SPIClass* spi = nullptr;
    static inline SubGhzService* subGhzService = nullptr;
    static inline char* commandBuffer = nullptr;
    static inline size_t commandLength = 0;
    static inline bool commandOverflow = false;
    static inline bool hardwareReady = false;
    static inline bool rxReporting = false;
    static inline uint32_t lastInputPollMs = 0;
};
