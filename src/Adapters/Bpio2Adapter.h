#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <cstddef>
#include <cstdint>

#include "Interfaces/IHostSerial.h"
#include "Interfaces/IInput.h"
#include "Models/Bpio2.h"

// Exclusive one-shot USB CDC adapter implementing the official BPIO2 v2
// FlatBuffers + COBS protocol used by current Bus Pirate host clients.

constexpr size_t BPIO2_IO_PIN_COUNT = 8;

// Logical BPIO pin mapping:
//   IO0 = SPI CS
//   IO1 = SPI CLK / I2C SCL
//   IO2 = SPI MOSI / I2C SDA
//   IO3 = SPI MISO
//   IO4..IO7 = auxiliary GPIOs
struct Bpio2AdapterConfig {
    uint8_t ioPins[BPIO2_IO_PIN_COUNT];
    uint32_t defaultSpiFrequency;
    uint32_t defaultI2cFrequency;
};


class Bpio2Adapter {
public:
    static void run(const Bpio2AdapterConfig& config, IInput& input, IHostSerial& hostSerial);

private:
    enum class Mode : uint8_t {
        HiZ,
        SPI,
        I2C,
    };

    static constexpr uint32_t SERIAL_BAUDRATE = 115200;
    static constexpr uint32_t MIN_SPI_FREQUENCY = 1000;
    static constexpr uint32_t MAX_SPI_FREQUENCY = 40000000;
    static constexpr uint32_t MIN_I2C_FREQUENCY = 1000;
    static constexpr uint32_t MAX_I2C_FREQUENCY = 1000000;
    static constexpr size_t MAX_I2C_TRANSFER = 128;
    static constexpr uint8_t SPI_RESERVED_MASK = 0x0F;
    static constexpr uint8_t I2C_RESERVED_MASK = 0x06;

    static constexpr size_t ENCODED_FRAME_CAPACITY = Bpio2::MAX_COBS_SIZE + 1;
    static constexpr size_t PACKET_BUFFER_OFFSET = ENCODED_FRAME_CAPACITY;
    static constexpr size_t I2C_PENDING_OFFSET = PACKET_BUFFER_OFFSET + Bpio2::MAX_PACKET_SIZE;
    static constexpr size_t WORKSPACE_SIZE = I2C_PENDING_OFFSET + MAX_I2C_TRANSFER;

    static inline Bpio2AdapterConfig* config = nullptr;
    static inline IHostSerial* hostSerial = nullptr;
    static inline SPIClass* spi = nullptr;
    static inline TwoWire* i2c = nullptr;
    static inline uint8_t* workspace = nullptr;

    static inline Mode mode = Mode::HiZ;
    static inline bool spiStarted = false;
    static inline bool i2cStarted = false;
    static inline bool bitOrderMsb = false;
    static inline bool chipSelectIdle = false;
    static inline uint8_t spiDataMode = 0;
    static inline uint32_t spiFrequency = 0;
    static inline uint32_t i2cFrequency = 0;
    static inline uint8_t ioDirection = 0;
    static inline uint8_t ioValue = 0;
    static inline size_t i2cPendingLength = 0;
    static inline bool i2cSequenceActive = false;

    static void allocateRuntimeResources(const Bpio2AdapterConfig& adapterConfig,
                                         IHostSerial& hostSerialRef);
    static void resetState();
    static void cleanup();
    static bool inputRequestedReset(IInput& input);

    static uint8_t* encodedFrameBuffer();
    static uint8_t* packetBuffer();
    static uint8_t* i2cPendingBuffer();

    static void processFrame(const uint8_t* encoded, size_t encodedFrameLength);
    static void processRequest(const Bpio2& request);
    static void sendDecodedResponse(const uint8_t* response, size_t responseLength);
    static void sendError(const char* error);
    static void sendConfigurationResponse(const char* error = nullptr);
    static void sendDataResponse(const uint8_t* data = nullptr,
                                 size_t length = 0,
                                 const char* error = nullptr);
    static void sendStatusResponse(const char* error = nullptr);

    static const char* handleConfiguration(const Bpio2::ConfigurationRequest& request);
    static const char* handleData(const Bpio2::DataRequest& request,
                                  uint8_t* readData,
                                  size_t& readLength);
    static const char* handleSpiData(const Bpio2::DataRequest& request,
                                     uint8_t* readData,
                                     size_t& readLength);
    static const char* handleI2cData(const Bpio2::DataRequest& request,
                                     uint8_t* readData,
                                     size_t& readLength);

    static const char* changeMode(Mode newMode, const Bpio2::ModeConfiguration& modeConfig);
    static void stopCurrentProtocol();
    static void makeAllPinsHiZ();
    static void applyAuxiliaryGpioState();
    static uint8_t reservedIoMask();
    static uint8_t effectiveDirectionMask();
    static uint8_t readIoValues();

    static void setChipSelect(bool asserted);
    static const char* currentModeName();
    static uint32_t clampFrequency(uint32_t frequency,
                                   uint32_t minimum,
                                   uint32_t maximum,
                                   uint32_t fallback);
    static void parseFirmwareVersion(uint8_t& major, uint8_t& minor);
};
