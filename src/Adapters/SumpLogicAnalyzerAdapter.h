#pragma once

#include <Arduino.h>
#include <array>
#include "Interfaces/IInput.h"
#include "Interfaces/IHostSerial.h"

// Exclusive one-shot USB CDC adapter for PulseView/sigrok SUMP capture.
// samples up to 8 GPIOs by polling

struct SumpLogicAnalyzerConfig {
    std::array<uint8_t, 8> pins;
    uint8_t channelCount;
    uint32_t sampleRate;
    uint32_t sampleCount;
};

struct SumpCaptureDebugInfo {
    uint32_t requestedDivider;
    uint32_t requestedSampleRate;
    uint32_t effectiveSampleRate;
    uint32_t sampleCount;
    bool fastPathUsed;
    uint64_t captureStartUs;
    uint64_t captureEndUs;
    uint64_t captureDurationUs;
    uint32_t captureDurationMs;
    uint32_t measuredSamplesPerSecond;
};

class SumpLogicAnalyzerAdapter {
public:
    static void run(const SumpLogicAnalyzerConfig& config, IInput& input, IHostSerial& hostSerial);
    static SumpCaptureDebugInfo getLastCaptureDebugInfo();

private:
    static constexpr uint8_t SUMP_RESET = 0x00;
    static constexpr uint8_t SUMP_RUN = 0x01;
    static constexpr uint8_t SUMP_ID = 0x02;
    static constexpr uint8_t SUMP_METADATA = 0x04;
    static constexpr uint8_t SUMP_XON = 0x11;
    static constexpr uint8_t SUMP_XOFF = 0x13;
    static constexpr uint8_t SUMP_SET_DIVIDER = 0x80;
    static constexpr uint8_t SUMP_SET_READ_DELAY = 0x81;
    static constexpr uint8_t SUMP_SET_FLAGS = 0x82;

    static constexpr uint32_t MIN_SAMPLE_RATE = 1;
    static constexpr uint32_t SUMP_CLOCK_HZ = 100000000UL;
    static constexpr uint32_t MAX_SAMPLE_RATE = 200000000UL;

    static constexpr uint32_t DEFAULT_SAMPLE_COUNT = 4096;
    static constexpr uint32_t MIN_CAPTURE_HEAP_RESERVE = 8 * 1024;
    static constexpr uint32_t MAX_PROTOCOL_SAMPLE_COUNT = 0x00FFFFFF;
    static constexpr uint8_t MAX_CHANNELS = 8;
    static constexpr uint32_t CAPTURE_ABORT_CHECK_INTERVAL = 256;
    static constexpr uint32_t UPLOAD_ABORT_CHECK_INTERVAL = 256;
    static constexpr uint32_t UPLOAD_WRITE_TIMEOUT_MS = 2000;
    static constexpr size_t UPLOAD_BUFFER_SIZE = 512;

    static void configure(const SumpLogicAnalyzerConfig& config, IInput& input);
    static void handleCommand(uint8_t command);
    static void handleRun();
    static void handleFlags(uint32_t flags);
    static void resetSumpState();
    static bool consumePendingReset();
    static bool uploadCaptureSamples();
    static bool writeAll(const uint8_t* data, size_t length);
    static uint32_t calculateMaxSampleCount();
    static void prepareFastGpioMap();
    static void writeMetadata();
    static void writeStringMetadata(uint8_t key, const char* value);
    static void writeUint32Metadata(uint8_t key, uint32_t value);
    static void writeUint8Metadata(uint8_t key, uint8_t value);
    static bool readCommandByte(uint8_t& value);
    static uint32_t readLe32();
    static uint16_t readLe16();
    static void discardBytes(uint8_t count);
    static void writeBe32(uint32_t value);
    static bool allocateSampleBuffer(uint32_t requestedSampleCount);
    static uint32_t calibrateCycleCounterHz();
    static void IRAM_ATTR waitUntilCycle(uint32_t targetCycle);
    static void IRAM_ATTR capture();
    static void captureTimed();
    static void IRAM_ATTR captureAsFastAsPossible();
    static uint8_t IRAM_ATTR readSample6Fast();
    static uint8_t IRAM_ATTR readSample6LowOnlyFast();
    static uint8_t IRAM_ATTR readSample8Fast();
    static uint8_t IRAM_ATTR readSample8LowOnlyFast();
    static bool inputRequestedReset();

    static inline SumpLogicAnalyzerConfig activeConfig = {};
    static inline IHostSerial* hostSerial = nullptr;
    static inline IInput* activeInput = nullptr;
    static inline uint32_t sampleRate = 1000000;
    static inline uint32_t sampleCount = 4096;
    static inline uint32_t maxSampleCount = 128 * 1024;
    static inline uint8_t channelCount = 8;
    static inline uint8_t enabledChannelGroups = 0x0F;
    static inline uint8_t commandBuffer[4] = {};
    static inline uint8_t* samples = nullptr;
    static inline uint32_t sampleCapacity = 0;
    static inline bool captureAborted = false;
    static inline uint32_t lastDebugRequestedDivider = 0;
    static inline uint32_t lastDebugRequestedSampleRate = 0;
    static inline uint32_t lastDebugEffectiveSampleRate = 0;
    static inline uint32_t lastDebugSampleCount = 0;
    static inline bool lastDebugFastPathUsed = false;
    static inline uint64_t lastDebugCaptureStartUs = 0;
    static inline uint64_t lastDebugCaptureEndUs = 0;
    static inline uint64_t lastDebugCaptureDurationUs = 0;
    static inline uint32_t lastDebugCaptureDurationMs = 0;
    static inline uint32_t lastDebugMeasuredSamplesPerSecond = 0;
    static inline uint32_t lowMask0 = 0;
    static inline uint32_t lowMask1 = 0;
    static inline uint32_t lowMask2 = 0;
    static inline uint32_t lowMask3 = 0;
    static inline uint32_t lowMask4 = 0;
    static inline uint32_t lowMask5 = 0;
    static inline uint32_t lowMask6 = 0;
    static inline uint32_t lowMask7 = 0;
    static inline uint32_t highMask0 = 0;
    static inline uint32_t highMask1 = 0;
    static inline uint32_t highMask2 = 0;
    static inline uint32_t highMask3 = 0;
    static inline uint32_t highMask4 = 0;
    static inline uint32_t highMask5 = 0;
    static inline uint32_t highMask6 = 0;
    static inline uint32_t highMask7 = 0;
    static inline bool hasHighPins = false;
};
