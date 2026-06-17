#pragma once

#include "Services/NvsService.h"
#include "Interfaces/IDeviceView.h"
#include "Interfaces/IInput.h"
#include "Adapters/UsbUartBridgeAdapter.h"
#include "Adapters/FlashromSerprogAdapter.h"
#include "Adapters/SumpLogicAnalyzerAdapter.h"
#include "Adapters/OpenOcdBusPirateAdapter.h"
#include "Adapters/AvrDudeBusPirateAdapter.h"
#include "Adapters/Bpio2Adapter.h"
#include "Adapters/InfraredToyAdapter.h"
#include "Adapters/SubGhzRawCdcAdapter.h"
#include "Interfaces/IHostSerial.h"

class BootModeConfigurator {
public:
    BootModeConfigurator(IDeviceView& deviceView, IInput& deviceInput, NvsService& nvsService, IHostSerial& hostSerial);

    bool configure();

private:
    void showOneShotBootMode(OneShotBootMode mode,
                             const UsbUartBridgeConfig& usbUartBridgeConfig,
                             const FlashromSerprogConfig& flashromSerprogConfig,
                             const AvrDudeBusPirateConfig& busPirateAvrdudeConfig,
                             const Bpio2AdapterConfig& bpio2Config,
                             const SumpLogicAnalyzerConfig& sumpLogicAnalyzerConfig,
                             const OpenOcdBusPirateConfig& openOcdBusPirateConfig,
                             const InfraredToyConfig& infraredToyConfig,
                             const SubGhzRawCdcConfig& subGhzRawCdcConfig);

    IDeviceView& deviceView;
    IInput& deviceInput;
    NvsService& nvsService;
    IHostSerial& hostSerial;
};
