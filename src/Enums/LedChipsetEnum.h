#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <FastLED.h>

// Enum is from FastLED, ESPIChipsets

class LedChipsetMapper {
public:
    struct ChipsetInfo {
        const char* name;
        bool usesClock;
    };

    static ESPIChipsets fromString(const std::string& name) {
        std::string lowered = toLower(name);
        for (const auto& entry : chipsetEntries) {
            if (lowered == entry.key) {
                return entry.chipset;
            }
        }
        return ESPIChipsets::APA102; // fallback
    }

    static std::string toString(ESPIChipsets chipset) {
        for (const auto& entry : chipsetEntries) {
            if (entry.chipset == chipset) {
                return entry.key;
            }
        }
        return "apa102"; // fallback
    }

    static std::vector<std::string> getAllChipsets() {
        std::vector<std::string> names;
        names.reserve(chipsetEntriesCount);
        for (const auto& entry : chipsetEntries) {
            names.push_back(entry.key);
        }
        return names;
    }

    static bool isClockBased(const std::string& name) {
        std::string lowered = toLower(name);
        for (const auto& entry : chipsetEntries) {
            if (lowered == entry.key) {
                return entry.info.usesClock;
            }
        }
        return true;
    }

    static std::string normalize(const std::string& name) {
        std::string lowered = toLower(name);
        for (const auto& entry : chipsetEntries) {
            if (lowered == entry.key) {
                return entry.key;
            }
        }
        return "apa102";
    }

private:
    static std::string toLower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(), ::tolower);
        return out;
    }

    struct ChipsetEntry {
        const char* key;
        ESPIChipsets chipset;
        ChipsetInfo info;
    };

    inline static constexpr ChipsetEntry chipsetEntries[] = {
        {"apa102",    APA102,    {"APA102",    true}},
        {"apa102hd",  APA102HD,  {"APA102HD",  true}},
        {"dotstar",   DOTSTAR,   {"DOTSTAR",   true}},
        {"dotstarhd", DOTSTARHD, {"DOTSTARHD", true}},
        {"lpd6803",   LPD6803,   {"LPD6803",   true}},
        {"lpd8806",   LPD8806,   {"LPD8806",   true}},
        {"ws2801",    WS2801,    {"WS2801",    true}},
        {"ws2803",    WS2803,    {"WS2803",    true}},
        // {"sm16716", SM16716, {"SM16716", true}}, // crash the app
        {"p9813",     P9813,     {"P9813",     true}},
        {"sk9822",    SK9822,    {"SK9822",    true}},
        {"sk9822hd",  SK9822HD,  {"SK9822HD",  true}},
        {"hd107",     HD107,     {"HD107",     true}},
        {"hd107hd",   HD107HD,   {"HD107HD",   true}},
    };
    inline static constexpr size_t chipsetEntriesCount =
        sizeof(chipsetEntries) / sizeof(chipsetEntries[0]);
};
