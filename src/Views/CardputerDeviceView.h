#ifdef DEVICE_CARDPUTER

#pragma once

#include "M5DeviceView.h"

// Used for standalone mode only, share the screen with the terminal view

class CardputerDeviceView : public M5DeviceView {
public:
    // All disabled except draw methods and getSharedSpiInstance
    void initialize() override {}
    void welcome(TerminalTypeEnum& /*terminalType*/, std::string& /*terminalInfos*/) override {}
    void logo() override {}
    void show(PinoutConfig& /*config*/) override {}
    void adapterMode(const std::string& /*adapterName*/, const std::string& /*description*/, const std::vector<std::string>& /*details*/) override {}
    void topBar(const std::string& /*title*/, bool /*submenu*/, bool /*searchBar*/) override {}
    void horizontalSelection(const std::vector<std::string>& /*options*/,
        uint16_t /*selectedIndex*/,
        const std::string& /*description1*/,
        const std::string& /*description2*/) override {}
        void loading() override {}
};

#endif // DEVICE_CARDPUTER
