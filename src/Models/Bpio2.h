#pragma once

// Data models and limits for the official Bus Pirate BPIO2 v2 wire format.
// Schema: https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/main/bpio.fbs

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>

class Bpio2 {
public:
    static constexpr uint8_t VERSION_MAJOR = 2;
    static constexpr uint16_t VERSION_MINOR = 2;

    static constexpr size_t MAX_WRITE_SIZE = 32 * 1024;
    static constexpr size_t MAX_READ_SIZE = 32 * 1024;

    static constexpr size_t MAX_PACKET_SIZE =
        MAX_READ_SIZE + 256;

    static constexpr size_t MAX_COBS_SIZE =
        MAX_PACKET_SIZE +
        ((MAX_PACKET_SIZE + 253) / 254) +
        1;

    enum class RequestType : uint8_t {
        None = 0,
        Status = 1,
        Configuration = 2,
        Data = 3,
    };

    enum class ResponseType : uint8_t {
        None = 0,
        Status = 1,
        Configuration = 2,
        Data = 3,
    };

    struct StringView {
        const char* data = nullptr;
        size_t length = 0;

        bool equalsIgnoreCase(const char* value) const {
            if (!value) {
                return false;
            }

            const size_t valueLength = std::strlen(value);
            if (valueLength != length) {
                return false;
            }

            for (size_t i = 0; i < length; ++i) {
                const unsigned char left = static_cast<unsigned char>(data[i]);
                const unsigned char right = static_cast<unsigned char>(value[i]);

                if (std::tolower(left) != std::tolower(right)) {
                    return false;
                }
            }

            return true;
        }

        bool empty() const {
            return length == 0;
        }
    };

    struct ModeConfiguration {
        uint32_t speed = 20000;
        uint8_t dataBits = 8;
        bool parity = false;
        uint8_t stopBits = 1;
        bool flowControl = false;
        bool signalInversion = false;
        bool clockStretch = false;
        bool clockPolarity = false;
        bool clockPhase = false;
        bool chipSelectIdle = true;
        uint8_t submode = 0;
        uint32_t txModulation = 0;
        uint8_t rxSensor = 0;
    };

    struct ConfigurationRequest {
        bool hasMode = false;
        StringView mode;
        bool hasModeConfiguration = false;
        ModeConfiguration modeConfiguration;

        bool modeBitOrderMsb = false;
        bool modeBitOrderLsb = false;
        bool psuDisable = false;
        bool psuEnable = false;
        bool pullupDisable = false;
        bool pullupEnable = false;

        bool hasIoDirectionMask = false;
        uint8_t ioDirectionMask = 0;
        uint8_t ioDirection = 0;
        bool hasIoValueMask = false;
        uint8_t ioValueMask = 0;
        uint8_t ioValue = 0;

        bool hasLedResume = false;
        bool hasLedColor = false;
        bool hasPrintString = false;
        bool hardwareBootloader = false;
        bool hardwareReset = false;
        bool hardwareSelftest = false;
    };

    struct DataRequest {
        bool startMain = false;
        bool startAlt = false;
        const uint8_t* dataWrite = nullptr;
        size_t dataWriteLength = 0;
        uint16_t bytesRead = 0;
        bool stopMain = false;
        bool stopAlt = false;
    };

    struct StatusSnapshot {
        uint8_t hardwareMajor = 1;
        uint8_t hardwareMinor = 0;
        uint8_t firmwareMajor = 0;
        uint8_t firmwareMinor = 0;
        const char* firmwareGitHash = "";
        const char* firmwareDate = "";

        const char* const* modesAvailable = nullptr;
        size_t modesAvailableCount = 0;
        const char* modeCurrent = "HiZ";
        const char* const* pinLabels = nullptr;
        size_t pinLabelCount = 0;
        bool bitOrderMsb = true;
        uint32_t maxPacketSize = MAX_PACKET_SIZE;
        uint32_t maxWrite = MAX_WRITE_SIZE;
        uint32_t maxRead = MAX_READ_SIZE;
        uint8_t ioDirection = 0;
        uint8_t ioValue = 0;
    };

    uint8_t versionMajor = 0;
    uint16_t minimumVersionMinor = 0;
    RequestType type = RequestType::None;
    ConfigurationRequest configuration;
    DataRequest data;
};
