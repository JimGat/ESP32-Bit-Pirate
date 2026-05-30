#pragma once
#include <string>
#include <vector>
#include <esp_system.h> 
#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Interfaces/IDeviceView.h"
#include "Managers/UserInputManager.h"
#include "Transformers/ArgTransformer.h"
#include "Services/SystemService.h"
#include "Services/LittleFsService.h"
#include "Services/WifiService.h"
#include "States/GlobalState.h"

class SysInfoShell {
public:
    SysInfoShell(ITerminalView& terminalView,
                 IInput& terminalInput,
                 IDeviceView& deviceView,
                 UserInputManager& userInputManager,
                 ArgTransformer& argTransformer,
                 SystemService& systemService,
                 LittleFsService& littleFsService,
                 WifiService& wifiService);

    void run();

private:
    inline static constexpr const char* actions[] = {
        "  📊 System summary",
        "  📟 Hardware info",
        "  📦 Memory",
        "  📺 Screen",
        "  🧩 Partitions",
        "  📁 LittleFS",
        "  🧰 NVS",
        "  🌐 Network",
        "  🐞 Debug",
        " 🔄 Reboot",
        " 🔌 Bootloader",
        " 🚪 Exit"
    };

    inline static constexpr size_t actionCount = sizeof(actions) / sizeof(actions[0]);

    const char* resetReasonToStr(int r) {
        switch (static_cast<esp_reset_reason_t>(r)) {
            case ESP_RST_POWERON:   return "Power-On";
            case ESP_RST_EXT:       return "External";
            case ESP_RST_SW:        return "Software";
            case ESP_RST_PANIC:     return "Panic";
            case ESP_RST_INT_WDT:   return "Interrupt WDT";
            case ESP_RST_TASK_WDT:  return "Task WDT";
            case ESP_RST_WDT:       return "Other WDT";
            case ESP_RST_DEEPSLEEP: return "Deep-Sleep Wake";
            case ESP_RST_BROWNOUT:  return "Brownout";
            case ESP_RST_SDIO:      return "SDIO";
            default:                return "Unknown";
        }
    }

    const char* flashModeToStr(int m) {
        switch (m) {
            case 0: return "QIO";
            case 1: return "QOUT";
            case 2: return "DIO";
            case 3: return "DOUT";
            case 4: return "FAST_READ";
            case 5: return "SLOW_READ";
            default:   return "?";
        }
    }

    // Actions
    void cmdSummary();
    void cmdHardwareInfo();
    void cmdMemory();
    void cmdPartitions();
    void cmdScreen();   
    void cmdFS();
    void cmdFSShell();
    void cmdNVS();
    void cmdNet();
    void cmdDebugLogs();
    void cmdReboot(bool hard = false);
    void cmdUsbBootloaderMode();

    void fsListFiles();
    void fsDeleteFile();
    void fsDeleteAll();

    ITerminalView&     terminalView;
    IInput&            terminalInput;
    IDeviceView&       deviceView;
    UserInputManager&  userInputManager;
    ArgTransformer&    argTransformer;
    SystemService&     systemService;
    LittleFsService&   littleFsService;
    WifiService&       wifiService;
    GlobalState&       state = GlobalState::getInstance();
};
