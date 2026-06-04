#pragma once
#include <string>
#include <vector>
#include <Models/TerminalCommand.h>

class TerminalCommandTransformer {
public:
    TerminalCommand transform(const std::string& raw) const;
    std::vector<TerminalCommand> transformMany(const std::string& raw) const;
    std::vector<TerminalCommand> transformRepeatCommand(const std::string& raw) const;
    bool isPipelineCommand(const std::string& raw) const;
    bool isMacroCommand(const std::string& raw) const;
    bool isBuiltinCommand(const std::string& raw) const;
    bool isRepeatCommand(const std::string& raw) const;
    bool isGlobalCommand(const TerminalCommand& cmd) const; 
    bool isScreenCommand(const TerminalCommand& cmd) const;
private:
    std::string normalizeRaw(const std::string& raw) const;
    void autoCorrectRoot(TerminalCommand& cmd) const;
    void autoCorrectSubCommand(TerminalCommand& cmd) const;
    int scoreTightEditDistance(const std::string& a, const char* b) const;

    // aliases for protocols
    struct Alias { const char* from; const char* to; };
    inline static const Alias aliases[] = {
        {"onewire",   "1wire"},
        {"1w",        "1wire"},
        {"twowire",   "2wire"},
        {"2w",        "2wire"},
        {"threewire", "3wire"},
        {"3w",        "3wire"},
        {"i2",        "i2c"},
        {"serial",    "uart"},
        {"hd",        "hduart"},
        {"eth",       "ethernet"},
        {"bt",        "bluetooth"},
        {"ble",       "bluetooth"},
        {"ir",        "infrared"},
        {"sub",       "subghz"},
        {"cc1101",    "subghz"},
        {"rf",        "rf24"},
        {"nrf",      "rf24"},
        {"nfc",       "rfid"},
        {"c5",        "expander"},
        {"exp",       "expander"},

        {nullptr, nullptr}
    };
};
