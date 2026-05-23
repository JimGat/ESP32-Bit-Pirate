#pragma once

#include <string>
#include <vector>
#include <algorithm>

enum class ModeEnum {
    None = -1,
    HIZ,
    OneWire,
    UART,
    HDUART,
    I2C,
    SPI,
    TwoWire,
    ThreeWire,
    DIO,
    LED,
    Infrared,
    USB,
    Bluetooth,
    WiFi,
    JTAG,
    I2S,
    CAN_,
    ETHERNET,
    SUBGHZ,
    RFID,
    RF24_,
    FM,
    CELL,
    EXPANDER,
    COUNT
};

class ModeEnumMapper {
public:
    struct ModeEntry {
        ModeEnum mode;
        const char* name;
    };

    inline static constexpr ModeEntry modeEntries[] = {
        {ModeEnum::None,      "None"},
        {ModeEnum::HIZ,       "HIZ"},
        {ModeEnum::OneWire,   "1WIRE"},
        {ModeEnum::UART,      "UART"},
        {ModeEnum::HDUART,    "HDUART"},
        {ModeEnum::I2C,       "I2C"},
        {ModeEnum::SPI,       "SPI"},
        {ModeEnum::TwoWire,   "2WIRE"},
        {ModeEnum::ThreeWire, "3WIRE"},
        {ModeEnum::DIO,       "DIO"},
        {ModeEnum::LED,       "LED"},
        {ModeEnum::Infrared,  "INFRARED"},
        {ModeEnum::USB,       "USB"},
        {ModeEnum::Bluetooth, "BLUETOOTH"},
        {ModeEnum::WiFi,      "WIFI"},
        {ModeEnum::JTAG,      "JTAG"},
        {ModeEnum::I2S,       "I2S"},
        {ModeEnum::CAN_,      "CAN"},
        {ModeEnum::ETHERNET,  "ETHERNET"},
        {ModeEnum::SUBGHZ,    "SUBGHZ"},
        {ModeEnum::RFID,      "RFID"},
        {ModeEnum::RF24_,     "RF24"},
        {ModeEnum::FM,        "FM"},
        {ModeEnum::CELL,      "CELL"},
        {ModeEnum::EXPANDER,  "EXPANDER"},
    };

    inline static constexpr size_t modeEntriesCount = sizeof(modeEntries) / sizeof(modeEntries[0]);

    static std::string toString(ModeEnum proto) {
        for (size_t i = 0; i < modeEntriesCount; ++i) {
            if (modeEntries[i].mode == proto) {
                return modeEntries[i].name;
            }
        }
        return "Unknown Protocol";
    }

    static std::vector<ModeEnum> getProtocols() {
        std::vector<ModeEnum> out;
        out.reserve(modeEntriesCount - 1);
        for (size_t i = 0; i < modeEntriesCount; ++i) {
            if (modeEntries[i].mode != ModeEnum::None) {
                out.push_back(modeEntries[i].mode);
            }
        }
        return out;
    }

    static std::vector<std::string> getProtocolNames(const std::vector<ModeEnum>& protocols) {
        std::vector<std::string> names;
        for (const auto& proto : protocols) {
            names.push_back(toString(proto));
        }
        return names;
    }

    static std::string toUpper(std::string s) {
        for (char& c : s) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return s;
    }

    static ModeEnum fromString(const std::string& name) {
        std::string upper = toUpper(name);

        for (size_t i = 0; i < modeEntriesCount; ++i) {
            if (modeEntries[i].name == upper) {
                return modeEntries[i].mode;
            }
        }
        return ModeEnum::None;
    }
};
