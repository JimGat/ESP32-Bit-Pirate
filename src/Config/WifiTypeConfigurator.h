#pragma once

#include <Enums/TerminalTypeEnum.h>
#include <Vendors/CardWifiSetup.h>
#include <Vendors/StickWifiSetup.h>
#include <Vendors/S3WifiSetup.h>
#include <Vendors/TembedWifiSetup.h>
#include <Vendors/TdisplayWifiSetup.h>
#include <Vendors/WaveshareS3GeekWifiSetup.h>
#include <Interfaces/IDeviceView.h>
#include <Interfaces/IInput.h>
#include <string>
#include <WiFi.h>

class WifiTypeConfigurator {
public:
    WifiTypeConfigurator(IDeviceView& view, IInput& input)
        : view(view), input(input) {}

    std::string configure(TerminalTypeEnum& terminalType);

private:
    IDeviceView& view;
    IInput& input;
};
