#include "BootModeConfigurator.h"
#include "States/GlobalState.h"
#include <algorithm>
#include <vector>

BootModeConfigurator::BootModeConfigurator(IDeviceView& deviceView, IInput& deviceInput, NvsService& nvsService)
    : deviceView(deviceView),
      deviceInput(deviceInput),
      nvsService(nvsService) {}

bool BootModeConfigurator::configure() {
    GlobalState& state = GlobalState::getInstance();
    UsbUartBridgeConfig usbUartBridgeConfig = {
        state.getUartRxPin(),
        state.getUartTxPin(),
        state.isUartInverted()
    };
    FlashromSerprogConfig flashromSerprogConfig = {
        state.getSpiCSPin(),
        state.getSpiCLKPin(),
        state.getSpiMISOPin(),
        state.getSpiMOSIPin(),
        state.getSpiFrequency()
    };
    AvrDudeBusPirateConfig busPirateAvrdudeConfig = {
        state.getSpiCSPin(),
        state.getSpiCLKPin(),
        state.getSpiMISOPin(),
        state.getSpiMOSIPin(),
        1000000
    };
    SumpLogicAnalyzerConfig sumpLogicAnalyzerConfig = {
        {
            state.getSpiCSPin(),
            state.getSpiCLKPin(),
            state.getSpiMISOPin(),
            state.getSpiMOSIPin(),
            0,
            0,
            0,
            0
        },
        4,
        1000000,
        4096
    };
    OpenOcdBusPirateConfig openOcdBusPirateConfig = {
        state.getSpiCLKPin(),
        state.getSpiCSPin(),
        state.getSpiMOSIPin(),
        state.getSpiMISOPin(),
        state.getSpiCLKPin(),
        state.getSpiMOSIPin()
    };

    nvsService.open();
    OneShotBootMode mode = nvsService.consumeOneShotBootMode();
    if (mode == OneShotBootMode::UsbUartBridge) {
        nvsService.getOneShotUsbUartBridgeConfig(
            state.getUartRxPin(),
            state.getUartTxPin(),
            state.isUartInverted(),
            usbUartBridgeConfig.uartRxPin,
            usbUartBridgeConfig.uartTxPin,
            usbUartBridgeConfig.uartInverted
        );
        nvsService.clearOneShotUsbUartBridgeConfig();
    }
    if (mode == OneShotBootMode::FlashromSerprog) {
        nvsService.getOneShotFlashromSerprogConfig(
            state.getSpiCSPin(),
            state.getSpiCLKPin(),
            state.getSpiMISOPin(),
            state.getSpiMOSIPin(),
            state.getSpiFrequency(),
            flashromSerprogConfig.csPin,
            flashromSerprogConfig.sckPin,
            flashromSerprogConfig.misoPin,
            flashromSerprogConfig.mosiPin,
            flashromSerprogConfig.frequency
        );
        nvsService.clearOneShotFlashromSerprogConfig();
    }
    if (mode == OneShotBootMode::AvrDudeBusPirate) {
        nvsService.getOneShotAvrDudeBusPirateConfig(
            state.getSpiCSPin(),
            state.getSpiCLKPin(),
            state.getSpiMISOPin(),
            state.getSpiMOSIPin(),
            1000000,
            busPirateAvrdudeConfig.csPin,
            busPirateAvrdudeConfig.sckPin,
            busPirateAvrdudeConfig.misoPin,
            busPirateAvrdudeConfig.mosiPin,
            busPirateAvrdudeConfig.frequency
        );
        nvsService.clearOneShotAvrDudeBusPirateConfig();
    }
    if (mode == OneShotBootMode::SumpLogicAnalyzer) {
        nvsService.getOneShotSumpLogicAnalyzerConfig(
            sumpLogicAnalyzerConfig.pins.data(),
            sumpLogicAnalyzerConfig.channelCount,
            sumpLogicAnalyzerConfig.channelCount
        );
        nvsService.clearOneShotSumpLogicAnalyzerConfig();
    }
    if (mode == OneShotBootMode::OpenOcdBusPirate) {
        nvsService.getOneShotOpenOcdBusPirateConfig(
            openOcdBusPirateConfig.tckPin,
            openOcdBusPirateConfig.tmsPin,
            openOcdBusPirateConfig.tdiPin,
            openOcdBusPirateConfig.tdoPin,
            openOcdBusPirateConfig.swclkPin,
            openOcdBusPirateConfig.swdioPin,
            openOcdBusPirateConfig.tckPin,
            openOcdBusPirateConfig.tmsPin,
            openOcdBusPirateConfig.tdiPin,
            openOcdBusPirateConfig.tdoPin,
            openOcdBusPirateConfig.swclkPin,
            openOcdBusPirateConfig.swdioPin
        );
        nvsService.clearOneShotOpenOcdBusPirateConfig();
    }
    nvsService.close();

    switch (mode) {
        case OneShotBootMode::UsbUartBridge:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, sumpLogicAnalyzerConfig, openOcdBusPirateConfig);
            UsbUartBridgeAdapter::run(usbUartBridgeConfig, deviceInput);
            return true;

        case OneShotBootMode::FlashromSerprog:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, sumpLogicAnalyzerConfig, openOcdBusPirateConfig);
            FlashromSerprogAdapter::run(flashromSerprogConfig, deviceInput);
            return true;

        case OneShotBootMode::AvrDudeBusPirate:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, sumpLogicAnalyzerConfig, openOcdBusPirateConfig);
            AvrDudeBusPirateAdapter::run(busPirateAvrdudeConfig, deviceInput);
            return true;

        case OneShotBootMode::SumpLogicAnalyzer:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, sumpLogicAnalyzerConfig, openOcdBusPirateConfig);
            SumpLogicAnalyzerAdapter::run(sumpLogicAnalyzerConfig, deviceInput);
            return true;

        case OneShotBootMode::OpenOcdBusPirate:
            showOneShotBootMode(mode, usbUartBridgeConfig, flashromSerprogConfig, busPirateAvrdudeConfig, sumpLogicAnalyzerConfig, openOcdBusPirateConfig);
            OpenOcdBusPirateAdapter::run(openOcdBusPirateConfig, deviceInput);
            return true;

        case OneShotBootMode::None:
        default:
            return false;
    }
}

void BootModeConfigurator::showOneShotBootMode(OneShotBootMode mode,
                                               const UsbUartBridgeConfig& usbUartBridgeConfig,
                                               const FlashromSerprogConfig& flashromSerprogConfig,
                                               const AvrDudeBusPirateConfig& busPirateAvrdudeConfig,
                                               const SumpLogicAnalyzerConfig& sumpLogicAnalyzerConfig,
                                               const OpenOcdBusPirateConfig& openOcdBusPirateConfig) {
    switch (mode) {
        case OneShotBootMode::UsbUartBridge:
            deviceView.adapterMode(
                "USB-UART Bridge",
                "CDC serial bridge",
                {
                    "RX GPIO " + std::to_string(usbUartBridgeConfig.uartRxPin),
                    "TX GPIO " + std::to_string(usbUartBridgeConfig.uartTxPin)
                }
            );
            break;

        case OneShotBootMode::FlashromSerprog:
            deviceView.adapterMode(
                "Flashrom SPI",
                "serprog SPI programmer",
                {
                    "CS GPIO " + std::to_string(flashromSerprogConfig.csPin),
                    "SCK GPIO " + std::to_string(flashromSerprogConfig.sckPin),
                    "MISO GPIO " + std::to_string(flashromSerprogConfig.misoPin),
                    "MOSI GPIO " + std::to_string(flashromSerprogConfig.mosiPin)
                }
            );
            break;

        case OneShotBootMode::AvrDudeBusPirate:
            deviceView.adapterMode(
                "AVRDUDE Bus Pirate",
                "AVR ISP binary SPI",
                {
                    "RESET/CS GPIO " + std::to_string(busPirateAvrdudeConfig.csPin),
                    "SCK GPIO " + std::to_string(busPirateAvrdudeConfig.sckPin),
                    "MISO GPIO " + std::to_string(busPirateAvrdudeConfig.misoPin),
                    "MOSI GPIO " + std::to_string(busPirateAvrdudeConfig.mosiPin),
                    "SCK " + std::to_string(busPirateAvrdudeConfig.frequency / 1000) + " kHz"
                }
            );
            break;

        case OneShotBootMode::SumpLogicAnalyzer: {
            std::vector<std::string> details;
            uint8_t channelCount = std::min<uint8_t>(sumpLogicAnalyzerConfig.channelCount, 8);
            details.reserve(channelCount);

            for (uint8_t i = 0; i < channelCount; ++i) {
                details.push_back(
                    "D" + std::to_string(i) + " GPIO " + std::to_string(sumpLogicAnalyzerConfig.pins[i])
                );
            }

            deviceView.adapterMode(
                "SUMP Logic",
                "PulseView/Sigrok OLS",
                details
            );
            break;
        }

        case OneShotBootMode::OpenOcdBusPirate:
            deviceView.adapterMode(
                "OpenOCD",
                "Bus Pirate JTAG/SWD",
                {
                    "TCK GPIO " + std::to_string(openOcdBusPirateConfig.tckPin),
                    "TMS GPIO " + std::to_string(openOcdBusPirateConfig.tmsPin),
                    "TDI GPIO " + std::to_string(openOcdBusPirateConfig.tdiPin),
                    "TDO GPIO " + std::to_string(openOcdBusPirateConfig.tdoPin),
                    "SWCLK GPIO " + std::to_string(openOcdBusPirateConfig.swclkPin),
                    "SWDIO GPIO " + std::to_string(openOcdBusPirateConfig.swdioPin)
                }
            );
            break;

        case OneShotBootMode::None:
        default:
            break;
    }
}
