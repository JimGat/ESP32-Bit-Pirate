#include "Bpio2Adapter.h"

#include <cstdio>
#include <cstring>
#include <new>

#include "Transformers/Bpio2Transformer.h"
#include "States/GlobalState.h"
#include "Inputs/InputKeys.h"


void Bpio2Adapter::run(const Bpio2AdapterConfig& adapterConfig,
                       IInput& input,
                       IHostSerial& hostSerialRef) {
    allocateRuntimeResources(adapterConfig, hostSerialRef);

    hostSerial->disableReboot();
    hostSerial->setRxBufferSize(Bpio2::MAX_COBS_SIZE + 64);
    hostSerial->begin(SERIAL_BAUDRATE);

    resetState();

    size_t encodedLength = 0;
    bool frameOverflow = false;

    while (true) {
        if (inputRequestedReset(input)) {
            cleanup();
            ESP.restart();
        }

        while (hostSerial->available() > 0) {
            const int value = hostSerial->read();
            if (value < 0) break;

            const uint8_t byte = static_cast<uint8_t>(value);
            if (byte == 0x00) {
                if (frameOverflow) {
                    sendError("Flatbuffer buffer overflow");
                } else if (encodedLength > 0) {
                    processFrame(encodedFrameBuffer(), encodedLength);
                }
                encodedLength = 0;
                frameOverflow = false;
                continue;
            }

            if (frameOverflow) continue;
            if (encodedLength >= Bpio2::MAX_COBS_SIZE) {
                frameOverflow = true;
                continue;
            }
            encodedFrameBuffer()[encodedLength++] = byte;
        }
    }
}

void Bpio2Adapter::allocateRuntimeResources(
    const Bpio2AdapterConfig& adapterConfig,
    IHostSerial& hostSerialRef
) {
    hostSerial = &hostSerialRef;
    config = new Bpio2AdapterConfig(adapterConfig);
    spi = new SPIClass(HSPI);
    i2c = &Wire;
    workspace = new uint8_t[WORKSPACE_SIZE];

    if (!config || !spi || !workspace) {
        ESP.restart();

        while (true) {
            delay(1000);
        }
    }

    std::memset(workspace, 0, WORKSPACE_SIZE);
}

uint8_t* Bpio2Adapter::encodedFrameBuffer() {
    return workspace;
}

uint8_t* Bpio2Adapter::packetBuffer() {
    return workspace + PACKET_BUFFER_OFFSET;
}

uint8_t* Bpio2Adapter::i2cPendingBuffer() {
    return workspace + I2C_PENDING_OFFSET;
}

void Bpio2Adapter::resetState() {
    mode = Mode::HiZ;
    spiStarted = false;
    i2cStarted = false;
    bitOrderMsb = true;
    chipSelectIdle = true;
    spiDataMode = SPI_MODE0;
    spiFrequency = clampFrequency(config->defaultSpiFrequency,
                                  MIN_SPI_FREQUENCY,
                                  MAX_SPI_FREQUENCY,
                                  1000000);
    i2cFrequency = clampFrequency(config->defaultI2cFrequency,
                                  MIN_I2C_FREQUENCY,
                                  MAX_I2C_FREQUENCY,
                                  100000);
    ioDirection = 0;
    ioValue = 0;
    i2cPendingLength = 0;
    i2cSequenceActive = false;
    makeAllPinsHiZ();
}

void Bpio2Adapter::cleanup() {
    stopCurrentProtocol();
    makeAllPinsHiZ();
    mode = Mode::HiZ;
}

bool Bpio2Adapter::inputRequestedReset(IInput& input) {
    return input.readChar() != KEY_NONE;
}

void Bpio2Adapter::processFrame(const uint8_t* encoded, size_t encodedFrameLength) {
    size_t decodedLength = 0;
    if (!Bpio2Transformer::cobsDecode(encoded,
                           encodedFrameLength,
                           packetBuffer(),
                           Bpio2::MAX_PACKET_SIZE,
                           decodedLength)) {
        sendError("COBS decode failed");
        return;
    }

    Bpio2 request;
    const char* error = nullptr;
    if (!Bpio2Transformer::decodeRequest(packetBuffer(), decodedLength, request, error)) {
        sendError(error ? error : "Invalid flatbuffer");
        return;
    }

    processRequest(request);
}

void Bpio2Adapter::processRequest(const Bpio2& request) {
    switch (request.type) {
        case Bpio2::RequestType::Status:
            sendStatusResponse();
            return;

        case Bpio2::RequestType::Configuration:
            sendConfigurationResponse(handleConfiguration(request.configuration));
            return;

        case Bpio2::RequestType::Data: {
            size_t readLength = 0;
            uint8_t* readData = encodedFrameBuffer();
            const char* error = handleData(request.data, readData, readLength);
            sendDataResponse(readData, error ? 0 : readLength, error);
            return;
        }

        case Bpio2::RequestType::None:
        default:
            sendError("Unknown BPIO request type");
            return;
    }
}

void Bpio2Adapter::sendDecodedResponse(const uint8_t* response, size_t responseLength) {
    if (!hostSerial || !response || responseLength == 0) return;

    uint8_t* encoded = encodedFrameBuffer();
    size_t responseEncodedLength = 0;
    if (!Bpio2Transformer::cobsEncode(response,
                           responseLength,
                           encoded,
                           Bpio2::MAX_COBS_SIZE,
                           responseEncodedLength)) {
        return;
    }

    encoded[responseEncodedLength++] = 0x00;
    hostSerial->write(encoded, responseEncodedLength);
    hostSerial->flush();
}

void Bpio2Adapter::sendError(const char* error) {
    uint8_t* response = packetBuffer();
    const size_t length = Bpio2Transformer::buildErrorResponse(
        response, Bpio2::MAX_PACKET_SIZE, error);
    sendDecodedResponse(response, length);
}

void Bpio2Adapter::sendConfigurationResponse(const char* error) {
    uint8_t* response = packetBuffer();
    const size_t length = Bpio2Transformer::buildConfigurationResponse(
        response, Bpio2::MAX_PACKET_SIZE, error);
    if (length == 0) {
        sendError("Failed to build configuration response");
        return;
    }
    sendDecodedResponse(response, length);
}

void Bpio2Adapter::sendDataResponse(const uint8_t* data, size_t length, const char* error) {
    uint8_t* response = packetBuffer();
    const size_t responseLength = Bpio2Transformer::buildDataResponse(
        response, Bpio2::MAX_PACKET_SIZE, data, length, error, false);
    if (responseLength == 0) {
        sendError("Failed to build data response");
        return;
    }
    sendDecodedResponse(response, responseLength);
}

void Bpio2Adapter::sendStatusResponse(const char* error) {
    const char* availableModes[3] = {"HiZ", "SPI", "I2C"};
    char labelsStorage[8][28] = {};
    const char* labels[8] = {};

    const char* roleNames[8] = {"IO0", "IO1", "IO2", "IO3", "IO4", "IO5", "IO6", "IO7"};
    if (mode == Mode::SPI) {
        roleNames[0] = "CS";
        roleNames[1] = "CLK";
        roleNames[2] = "MOSI";
        roleNames[3] = "MISO";
    } else if (mode == Mode::I2C) {
        roleNames[1] = "SCL";
        roleNames[2] = "SDA";
    }

    for (size_t i = 0; i < 8; ++i) {
        std::snprintf(labelsStorage[i], sizeof(labelsStorage[i]), "%s GPIO %u",
                      roleNames[i], static_cast<unsigned>(config->ioPins[i]));
        labels[i] = labelsStorage[i];
    }

    uint8_t firmwareMajor = 0;
    uint8_t firmwareMinor = 0;
    parseFirmwareVersion(firmwareMajor, firmwareMinor);

    Bpio2::StatusSnapshot status;
    status.hardwareMajor = 1;
    status.hardwareMinor = 0;
    status.firmwareMajor = firmwareMajor;
    status.firmwareMinor = firmwareMinor;
    status.firmwareGitHash = "";
    status.firmwareDate = __DATE__;
    status.modesAvailable = availableModes;
    status.modesAvailableCount = 3;
    status.modeCurrent = currentModeName();
    status.pinLabels = labels;
    status.pinLabelCount = 8;
    status.bitOrderMsb = bitOrderMsb;
    status.maxPacketSize = Bpio2::MAX_PACKET_SIZE;
    status.maxWrite = mode == Mode::I2C ? MAX_I2C_TRANSFER :
                      mode == Mode::SPI ? Bpio2::MAX_WRITE_SIZE : 0;
    status.maxRead = mode == Mode::I2C ? MAX_I2C_TRANSFER :
                     mode == Mode::SPI ? Bpio2::MAX_READ_SIZE : 0;
    status.ioDirection = effectiveDirectionMask();
    status.ioValue = readIoValues();

    uint8_t* response = packetBuffer();
    const size_t length = Bpio2Transformer::buildStatusResponse(
        response, Bpio2::MAX_PACKET_SIZE, status, error);
    if (length == 0) {
        sendError("Failed to build status response");
        return;
    }
    sendDecodedResponse(response, length);
}

const char* Bpio2Adapter::handleConfiguration(const Bpio2::ConfigurationRequest& request) {
    if (request.psuEnable) return "PSU control is not supported";
    if (request.pullupEnable) return "Dock pull-up control is not supported";
    if (request.hasLedResume || request.hasLedColor) return "LED control is not supported";
    if (request.hasPrintString) return "Terminal print forwarding is not supported";
    if (request.hardwareBootloader) return "Bootloader request is not supported";
    if (request.hardwareReset) return "Hardware reset request is not supported";
    if (request.hardwareSelftest) return "Hardware self-test is not supported";
    if (request.modeBitOrderMsb && request.modeBitOrderLsb) {
        return "Conflicting bit-order settings";
    }

    if (request.modeBitOrderMsb) bitOrderMsb = true;
    if (request.modeBitOrderLsb) bitOrderMsb = false;

    if (request.hasMode) {
        Mode requestedMode;
        if (request.mode.equalsIgnoreCase("HiZ")) {
            requestedMode = Mode::HiZ;
        } else if (request.mode.equalsIgnoreCase("SPI")) {
            requestedMode = Mode::SPI;
        } else if (request.mode.equalsIgnoreCase("I2C")) {
            requestedMode = Mode::I2C;
        } else {
            return "Invalid mode name";
        }

        const char* modeError = changeMode(requestedMode, request.modeConfiguration);
        if (modeError) return modeError;
    }

    const uint8_t usableMask = static_cast<uint8_t>(~reservedIoMask());
    if (request.hasIoValueMask) {
        const uint8_t mask = request.ioValueMask & usableMask;
        ioValue = static_cast<uint8_t>((ioValue & ~mask) | (request.ioValue & mask));
    }
    if (request.hasIoDirectionMask) {
        const uint8_t mask = request.ioDirectionMask & usableMask;
        ioDirection = static_cast<uint8_t>((ioDirection & ~mask) | (request.ioDirection & mask));
    }
    applyAuxiliaryGpioState();

    return nullptr;
}

const char* Bpio2Adapter::handleData(const Bpio2::DataRequest& request,
                                     uint8_t* readData,
                                     size_t& readLength) {
    readLength = 0;
    if (mode == Mode::SPI) {
        return handleSpiData(request, readData, readLength);
    }
    if (mode == Mode::I2C) {
        return handleI2cData(request, readData, readLength);
    }
    return "No BPIO handler for current mode";
}

const char* Bpio2Adapter::handleSpiData(const Bpio2::DataRequest& request,
                                        uint8_t* readData,
                                        size_t& readLength) {
    if (!spiStarted) return "SPI is not initialized";

    const bool duplex = request.startAlt;
    const size_t expectedRead = duplex ? request.dataWriteLength + request.bytesRead : request.bytesRead;
    if (expectedRead > Bpio2::MAX_READ_SIZE) return "Data read size too large";

    if (request.startMain || request.startAlt) {
        setChipSelect(true);
    }

    SPISettings settings(spiFrequency,
                         bitOrderMsb ? MSBFIRST : LSBFIRST,
                         spiDataMode);
    spi->beginTransaction(settings);

    for (size_t i = 0; i < request.dataWriteLength; ++i) {
        const uint8_t received = spi->transfer(request.dataWrite[i]);
        if (duplex) readData[readLength++] = received;
    }
    for (uint16_t i = 0; i < request.bytesRead; ++i) {
        readData[readLength++] = spi->transfer(0xFF);
    }

    spi->endTransaction();

    if (request.stopMain || request.stopAlt) {
        setChipSelect(false);
    }
    return nullptr;
}

const char* Bpio2Adapter::handleI2cData(const Bpio2::DataRequest& request,
                                        uint8_t* readData,
                                        size_t& readLength) {
    if (!i2cStarted) return "I2C is not initialized";
    if (request.dataWriteLength > MAX_I2C_TRANSFER || request.bytesRead > MAX_I2C_TRANSFER) {
        return "I2C transfer too large";
    }

    if (request.startMain || request.startAlt) {
        i2cPendingLength = 0;
        i2cSequenceActive = true;
    }

    if (request.dataWriteLength > 0) {
        if (!i2cSequenceActive && i2cPendingLength == 0) {
            i2cSequenceActive = true;
        }
        if (request.dataWriteLength > MAX_I2C_TRANSFER - i2cPendingLength) {
            return "I2C pending write too large";
        }
        std::memcpy(i2cPendingBuffer() + i2cPendingLength,
                    request.dataWrite,
                    request.dataWriteLength);
        i2cPendingLength += request.dataWriteLength;
    }

    const bool stopRequested = request.stopMain || request.stopAlt;
    const bool executeNow = request.bytesRead > 0 || stopRequested;
    if (!executeNow) {
        return nullptr;
    }

    if (i2cPendingLength == 0) {
        i2cSequenceActive = false;
        return request.bytesRead == 0 ? nullptr : "I2C address byte missing";
    }

    const uint8_t addressByte = i2cPendingBuffer()[0];
    const uint8_t address = static_cast<uint8_t>(addressByte >> 1);
    const uint8_t* payload = i2cPendingBuffer() + 1;
    const size_t payloadLength = i2cPendingLength - 1;

    if (request.bytesRead > 0) {
        if (payloadLength > 0) {
            i2c->beginTransmission(address);
            const size_t written = i2c->write(payload, payloadLength);
            if (written != payloadLength) {
                i2cPendingLength = 0;
                i2cSequenceActive = false;
                return "I2C write buffer overflow";
            }
            const uint8_t result = i2c->endTransmission(false);
            if (result != 0) {
                i2cPendingLength = 0;
                i2cSequenceActive = false;
                return "I2C write phase failed";
            }
        }

        // Arduino Wire cannot emit a standalone STOP after requestFrom(..., false).
        // Complete the read safely here; a later stop-only BPIO2 request becomes a no-op.
        const size_t received = i2c->requestFrom(address,
                                                static_cast<size_t>(request.bytesRead),
                                                true);
        while (i2c->available() && readLength < request.bytesRead) {
            readData[readLength++] = static_cast<uint8_t>(i2c->read());
        }
        if (received != request.bytesRead || readLength != request.bytesRead) {
            i2cPendingLength = 0;
            i2cSequenceActive = false;
            return "I2C read failed";
        }
    } else {
        i2c->beginTransmission(address);
        if (payloadLength > 0) {
            const size_t written = i2c->write(payload, payloadLength);
            if (written != payloadLength) {
                i2cPendingLength = 0;
                i2cSequenceActive = false;
                return "I2C write buffer overflow";
            }
        }
        const uint8_t result = i2c->endTransmission(stopRequested);
        if (result != 0) {
            i2cPendingLength = 0;
            i2cSequenceActive = false;
            return "I2C transaction failed";
        }
    }

    i2cPendingLength = 0;
    if (request.bytesRead > 0 || stopRequested) i2cSequenceActive = false;
    return nullptr;
}

const char* Bpio2Adapter::changeMode(Mode newMode, const Bpio2::ModeConfiguration& modeConfig) {
    if (newMode == Mode::SPI && modeConfig.dataBits != 8) {
        return "Only 8-bit SPI transfers are supported";
    }

    stopCurrentProtocol();
    makeAllPinsHiZ();
    mode = newMode;

    if (newMode == Mode::HiZ) {
        applyAuxiliaryGpioState();
        return nullptr;
    }

    if (newMode == Mode::SPI) {
        spiFrequency = clampFrequency(modeConfig.speed,
                                      MIN_SPI_FREQUENCY,
                                      MAX_SPI_FREQUENCY,
                                      config->defaultSpiFrequency);
        chipSelectIdle = modeConfig.chipSelectIdle;
        spiDataMode = static_cast<uint8_t>((modeConfig.clockPolarity ? 0x02 : 0x00) |
                                           (modeConfig.clockPhase ? 0x01 : 0x00));

        pinMode(config->ioPins[0], OUTPUT);
        digitalWrite(config->ioPins[0], chipSelectIdle ? HIGH : LOW);
        spi->begin(config->ioPins[1], config->ioPins[3], config->ioPins[2], config->ioPins[0]);
        spiStarted = true;
        applyAuxiliaryGpioState();
        return nullptr;
    }

    i2cFrequency = clampFrequency(modeConfig.speed,
                                  MIN_I2C_FREQUENCY,
                                  MAX_I2C_FREQUENCY,
                                  config->defaultI2cFrequency);
    if (!i2c->begin(config->ioPins[2], config->ioPins[1], i2cFrequency)) {
        mode = Mode::HiZ;
        makeAllPinsHiZ();
        return "I2C initialization failed";
    }
    i2cStarted = true;
    i2cPendingLength = 0;
    i2cSequenceActive = false;
    applyAuxiliaryGpioState();
    return nullptr;
}

void Bpio2Adapter::stopCurrentProtocol() {
    if (spiStarted) {
        setChipSelect(false);
        spi->end();
        spiStarted = false;
    }
    if (i2cStarted) {
        i2c->end();
        i2cStarted = false;
    }
    i2cPendingLength = 0;
    i2cSequenceActive = false;
}

void Bpio2Adapter::makeAllPinsHiZ() {
    for (size_t i = 0; i < BPIO2_IO_PIN_COUNT; ++i) {
        pinMode(config->ioPins[i], INPUT);
    }
}

void Bpio2Adapter::applyAuxiliaryGpioState() {
    const uint8_t usableMask = static_cast<uint8_t>(~reservedIoMask());
    for (size_t i = 0; i < BPIO2_IO_PIN_COUNT; ++i) {
        const uint8_t bit = static_cast<uint8_t>(1u << i);
        if ((usableMask & bit) == 0) continue;

        // Set output latch before switching direction to avoid glitches.
        digitalWrite(config->ioPins[i], (ioValue & bit) ? HIGH : LOW);
        pinMode(config->ioPins[i], (ioDirection & bit) ? OUTPUT : INPUT);
    }
}

uint8_t Bpio2Adapter::reservedIoMask() {
    switch (mode) {
        case Mode::SPI: return SPI_RESERVED_MASK;
        case Mode::I2C: return I2C_RESERVED_MASK;
        case Mode::HiZ:
        default: return 0;
    }
}

uint8_t Bpio2Adapter::effectiveDirectionMask() {
    uint8_t result = static_cast<uint8_t>(ioDirection & ~reservedIoMask());
    if (mode == Mode::SPI) {
        result |= 0x07; // CS, CLK and MOSI are outputs; MISO is input.
    }
    return result;
}

uint8_t Bpio2Adapter::readIoValues() {
    uint8_t value = 0;
    for (size_t i = 0; i < BPIO2_IO_PIN_COUNT; ++i) {
        if (digitalRead(config->ioPins[i]) == HIGH) {
            value |= static_cast<uint8_t>(1u << i);
        }
    }
    return value;
}

void Bpio2Adapter::setChipSelect(bool asserted) {
    if (mode != Mode::SPI) return;
    const bool level = asserted ? !chipSelectIdle : chipSelectIdle;
    digitalWrite(config->ioPins[0], level ? HIGH : LOW);
}

const char* Bpio2Adapter::currentModeName() {
    switch (mode) {
        case Mode::SPI: return "SPI";
        case Mode::I2C: return "I2C";
        case Mode::HiZ:
        default: return "HiZ";
    }
}

uint32_t Bpio2Adapter::clampFrequency(uint32_t frequency,
                                             uint32_t minimum,
                                             uint32_t maximum,
                                             uint32_t fallback) {
    if (frequency == 0) return fallback;
    if (frequency < minimum) return minimum;
    if (frequency > maximum) return maximum;
    return frequency;
}

void Bpio2Adapter::parseFirmwareVersion(uint8_t& major, uint8_t& minor) {
    major = 0;
    minor = 0;
    const char* version = GlobalState::getInstance().getVersion();
    if (!version) return;

    unsigned parsedMajor = 0;
    unsigned parsedMinor = 0;
    if (std::sscanf(version, "%u.%u", &parsedMajor, &parsedMinor) >= 1) {
        major = static_cast<uint8_t>(parsedMajor > 255u ? 255u : parsedMajor);
        minor = static_cast<uint8_t>(parsedMinor > 255u ? 255u : parsedMinor);
    }
}
