#pragma once
#include <string>
#include <algorithm>
#include <vector>

enum class LedProtocolEnum {
    NEOPIXEL,
    WS2812, WS2812B, WS2811, WS2811_400, WS2813, WS2815, WS2816, WS2852,
    GS1903, SK6812, SK6822, APA104, APA106, PL9823,
    GE8822, GW6205, GW6205_400, LPD1886, LPD1886_8BIT,
    SM16703, TM1829, TM1812, TM1809, TM1804, TM1803,
    UCS1903, UCS1903B, UCS1904, UCS2903, UCS1912,
    LPD6803, LPD8806, SM16716, P9813,
    UNKNOWN
};

class LedProtocolEnumMapper {
public:
    static LedProtocolEnum fromString(const std::string& str) {
        std::string lowered = toLower(str);
        for (const auto& entry : protocolEntries) {
            if (lowered == entry.name) return entry.protocol;
        }
        return LedProtocolEnum::UNKNOWN;
    }

    static std::string toString(LedProtocolEnum protocol) {
        for (const auto& entry : protocolEntries) {
            if (entry.protocol == protocol) return entry.name;
        }
        return "unknown";
    }

    static std::vector<std::string> getAllProtocols() {
        std::vector<std::string> protocols;
        protocols.reserve(protocolEntriesCount);
        for (const auto& entry : protocolEntries) {
            if (entry.protocol != LedProtocolEnum::UNKNOWN) {
                protocols.push_back(entry.name);
            }
        }
        return protocols;
    }
    
private:
    static std::string toLower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(), ::tolower);
        return out;
    }

    struct ProtocolEntry {
        const char* name;
        LedProtocolEnum protocol;
    };

    inline static constexpr ProtocolEntry protocolEntries[] = {
        {"neopixel", LedProtocolEnum::NEOPIXEL},
        {"ws2812", LedProtocolEnum::WS2812},
        {"ws2812b", LedProtocolEnum::WS2812B},
        {"ws2811", LedProtocolEnum::WS2811},
        {"ws2811_400", LedProtocolEnum::WS2811_400},
        {"ws2813", LedProtocolEnum::WS2813},
        {"ws2815", LedProtocolEnum::WS2815},
        {"ws2816", LedProtocolEnum::WS2816},
        {"ws2852", LedProtocolEnum::WS2852},
        {"gs1903", LedProtocolEnum::GS1903},
        {"sk6812", LedProtocolEnum::SK6812},
        {"sk6822", LedProtocolEnum::SK6822},
        {"apa104", LedProtocolEnum::APA104},
        {"apa106", LedProtocolEnum::APA106},
        {"pl9823", LedProtocolEnum::PL9823},
        {"ge8822", LedProtocolEnum::GE8822},
        {"gw6205", LedProtocolEnum::GW6205},
        {"gw6205_400", LedProtocolEnum::GW6205_400},
        {"lpd1886", LedProtocolEnum::LPD1886},
        {"lpd1886_8bit", LedProtocolEnum::LPD1886_8BIT},
        {"sm16703", LedProtocolEnum::SM16703},
        {"tm1829", LedProtocolEnum::TM1829},
        {"tm1812", LedProtocolEnum::TM1812},
        {"tm1809", LedProtocolEnum::TM1809},
        {"tm1804", LedProtocolEnum::TM1804},
        {"tm1803", LedProtocolEnum::TM1803},
        {"ucs1903", LedProtocolEnum::UCS1903},
        {"ucs1903b", LedProtocolEnum::UCS1903B},
        {"ucs1904", LedProtocolEnum::UCS1904},
        {"ucs2903", LedProtocolEnum::UCS2903},
        {"ucs1912", LedProtocolEnum::UCS1912},
        {"lpd6803", LedProtocolEnum::LPD6803},
        {"lpd8806", LedProtocolEnum::LPD8806},
        {"sm16716", LedProtocolEnum::SM16716},
        {"p9813", LedProtocolEnum::P9813},
        {"unknown", LedProtocolEnum::UNKNOWN}
    };
    inline static constexpr size_t protocolEntriesCount =
        sizeof(protocolEntries) / sizeof(protocolEntries[0]);
};
