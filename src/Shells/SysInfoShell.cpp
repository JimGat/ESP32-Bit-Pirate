#include "SysInfoShell.h"
#include <cstdarg>
#include <cstdio>
#include <esp_chip_info.h>

SysInfoShell::SysInfoShell(ITerminalView& tv,
                           IInput& in,
                           IDeviceView& deviceView,
                           UserInputManager& uim,
                           ArgTransformer& at,
                           SystemService& sys,
                           LittleFsService& littleFsService,
                           WifiService& wifi)
    : terminalView(tv)
    , terminalInput(in)
    , deviceView(deviceView)
    , userInputManager(uim)
    , argTransformer(at)
    , systemService(sys)
    , littleFsService(littleFsService)  
    , wifiService(wifi) {}

void SysInfoShell::run() {
    bool loop = true;
    while (loop) {
        terminalView.println("\n=== System Shell ===");

        int choice = userInputManager.readValidatedChoiceIndex("Select action", actions, actionCount, actionCount - 1);

        switch (choice) {
            case 0: cmdSummary(); break;
            case 1: cmdHardwareInfo(); break;
            case 2: cmdMemory(); break;
            case 3: cmdScreen(); break;
            case 4: cmdPartitions(); break;
            case 5: cmdFS(); break;
            case 6: cmdNVS(); break;
            case 7: cmdNet(); break;
            case 8: cmdDebugLogs(); break;
            case 9: cmdReboot(); break;
            case 10: cmdUsbBootloaderMode(); break;
            case 11: // Exit
            default:
                loop = false;
                break;
        }
    }

    terminalView.println("Exiting System Shell...\n");
}

void SysInfoShell::cmdSummary() {
    terminalView.println("\n=== System Summary ===");
    terminalView.println("Model         : " + systemService.getChipModel());
    terminalView.println("Uptime        : " + std::to_string(systemService.getUptimeSeconds()) + " s");
    terminalView.println("Temperature   : " + systemService.getInternalTemperatureCStr() + " °C");
    terminalView.println("Screen        : " + std::to_string((deviceView.getBrightness() * 100) / 255) + " % bright");
    
    const int rr = systemService.getResetReason();
    terminalView.println(std::string("Reset reason  : ") + resetReasonToStr(rr) + " (" + std::to_string(rr) + ")");
    
    terminalView.println("Stack total   : " + std::to_string(systemService.getStackTotal()   / 1024) + " KB");
    terminalView.println("Heap total    : " + std::to_string(systemService.getHeapTotal()   / 1024) + " KB");
    terminalView.println("PSRAM total   : " + std::to_string(systemService.getPsramTotal()  / 1024) + " KB");
    terminalView.println("Flash total   : " + std::to_string(systemService.getFlashSizeBytes() / 1024) + " KB");

    terminalView.println("Firmware      : " + (std::string("version ") + state.getVersion()));
    terminalView.println("Build date    : " + std::string(__DATE__) + " " + std::string(__TIME__));
    terminalView.println("Infrared      : " + systemService.getInfraredBackend());
    terminalView.println("IDF version   : " + systemService.getIdfVersion());
    terminalView.println("Arduino core  : " + systemService.getArduinoCore());
}

void SysInfoShell::cmdHardwareInfo() {
    terminalView.println("\n=== Hardware Info ===");

    // Chip
    terminalView.println("Model             : " + systemService.getChipModel());
    terminalView.println("CPU cores         : " + std::to_string(systemService.getChipCores()));
    terminalView.println("CPU freq          : " + std::to_string(systemService.getCpuFreqMhz()) + " MHz");
    terminalView.println("CPU temp          : " + systemService.getInternalTemperatureCStr() + " °C");

    // Features (WiFi/BT/BLE)
    const uint32_t f = systemService.getChipFeaturesRaw();
    std::string features;
    if (f & CHIP_FEATURE_WIFI_BGN) features += "WiFi ";
    if (f & CHIP_FEATURE_BT)       features += "BT ";
    if (f & CHIP_FEATURE_BLE)      features += "BLE ";
    if (features.empty())          features = "?";
    terminalView.println("Features          : " + features);

    terminalView.println("Revision          : " + std::to_string(systemService.getChipRevision()));
    const int fullrev = systemService.getChipFullRevision();
    if (fullrev >= 0) {
        terminalView.println("Full revision     : " + std::to_string(fullrev));
    }

    // Flash
    terminalView.println("Flash total       : " + std::to_string(systemService.getFlashSizeBytes() / 1024) + " KB");
    terminalView.println("Flash speed       : " + std::to_string(systemService.getFlashSpeedHz() / 1000000U) + " MHz");
    terminalView.println("Flash mode        : " + std::string(flashModeToStr(systemService.getFlashModeRaw())));
    terminalView.println("Flash chip ID     : " + systemService.getFlashJedecIdHex());

    // Sketch
    const size_t sku = systemService.getSketchUsedBytes();   // used
    const size_t skl = systemService.getSketchFreeBytes();   // free
    const size_t skt = sku + skl;                            // total
    const int pctInt = skt ? static_cast<int>((sku * 100.0f / skt) + 0.5f) : 0;

    terminalView.println("Sketch total      : " + std::to_string(skt / 1024) + " KB");
    terminalView.println("Sketch free       : " + std::to_string(skl / 1024) + " KB");
    terminalView.println("Sketch usage      : " + std::to_string(pctInt) + " %");
    terminalView.println("Sketch MD5        : " + systemService.getSketchMD5());
}

void SysInfoShell::cmdMemory() {
    terminalView.println("\n=== Memory ===");
    
    // Stack
    char line[96];
    size_t stTot  = systemService.getStackTotal();
    float totKB  = stTot  / 1024.0f;
    snprintf(line, sizeof(line), "Stack total       : %.2f KB", totKB);
    terminalView.println(line);

    size_t stUsed = systemService.getStackUsed();
    float usedKB = stUsed / 1024.0f;
    float freeKB = (stTot > stUsed ? (stTot - stUsed) : 0) / 1024.0f;
    float pct    = (stUsed * 100.0f) / (float)stTot;
    int   stPct  = (stUsed * 100.0 / stTot) + 0.5;
    snprintf(line, sizeof(line), "Stack free        : %.2f KB", freeKB);
    terminalView.println(line);
    snprintf(line, sizeof(line), "Stack used        : %.2f KB (%.0f%%)", usedKB, pct);
    terminalView.println(line);

    // Heap
    const size_t heapTotal = systemService.getHeapTotal();
    const size_t heapFree  = systemService.getHeapFree();
    const int    heapPct   = heapTotal ? static_cast<int>(((heapTotal - heapFree) * 100.0f / heapTotal) + 0.5f) : 0;
    terminalView.println("Heap total        : " + std::to_string(heapTotal / 1024) + " KB");
    terminalView.println("Heap free         : " + std::to_string(heapFree / 1024) + " KB");
    terminalView.println("Heap used         : " + std::to_string((heapTotal - heapFree) / 1024) + " KB (" + std::to_string(heapPct) + "%)");
    terminalView.println("Min free heap     : " + std::to_string(systemService.getHeapMinFree()  / 1024) + " KB");
    terminalView.println("Max alloc heap    : " + std::to_string(systemService.getHeapMaxAlloc() / 1024) + " KB");

    // PSRAM
    const size_t psramTotal = systemService.getPsramTotal();
    const size_t psramFree  = systemService.getPsramFree();
    const int    psramPct   = psramTotal ? static_cast<int>(((psramTotal - psramFree) * 100.0f / psramTotal) + 0.5f) : 0;
    terminalView.println("PSRAM total       : " + std::to_string(psramTotal / 1024) + " KB");
    terminalView.println("PSRAM free        : " + std::to_string(psramFree / 1024) + " KB");
    terminalView.println("PSRAM used        : " + std::to_string((psramTotal - psramFree) / 1024) + " KB (" + std::to_string(psramPct) + "%)");
    terminalView.println("Min free PSRAM    : " + std::to_string(systemService.getPsramMinFree()  / 1024) + " KB");
    terminalView.println("Max alloc PSRAM   : " + std::to_string(systemService.getPsramMaxAlloc() / 1024) + " KB");
}

void SysInfoShell::cmdScreen() {
    terminalView.println("\n=== Device Screen ===");

    uint8_t brightness = deviceView.getBrightness();

    // Normalize to percent
    uint8_t percent = static_cast<uint8_t>((brightness * 100) / 255);
    terminalView.println("Brightness : current " + std::to_string(percent) + " %");

    // Ask user new value
    uint8_t newPercent = userInputManager.readValidatedUint8(
        "Set brightness (0-100%)",
        percent,
        0,
        100
    );

    // Convert
    uint8_t newBrightness = static_cast<uint8_t>((newPercent * 255) / 100);

    deviceView.setBrightness(newBrightness);

    terminalView.println("Brightness updated to " +
                         std::to_string(newPercent) + " %");
}

void SysInfoShell::cmdPartitions() {
    terminalView.println("\n=== Partitions ===");
    terminalView.println(systemService.getPartitions());
}

void SysInfoShell::cmdFS() {
    terminalView.println("\n=== LittleFS ===");
    
    if (littleFsService.begin()) { // autoformat if failed mounted
        size_t total, used;
        if (!littleFsService.getSpace(total, used)) {
            terminalView.println("Failed to get LittleFS space info.");
            return;
        }
        const size_t freeB = (total >= used) ? (total - used) : 0;
        const size_t fileCount = littleFsService.listFiles("/", "*").size();

        terminalView.println("LittleFS mounted successfully.");
        terminalView.println("Total  : " + std::to_string(total / 1024) + " KB");
        terminalView.println("Used   : " + std::to_string(used  / 1024) + " KB");
        terminalView.println("Free   : " + std::to_string(freeB / 1024) + " KB");
        terminalView.println("Files  : " + std::to_string(fileCount));
        
        if (fileCount == 0) {
            return;
        }
        
        terminalView.println("");
        auto confirm = userInputManager.readYesNo("Enter LittleFS manager?", false);
        if (confirm) {
            cmdFSShell();
        }

        // webui will continue using it
        // systemService.littlefsEnd();
    } else {
        terminalView.println("LittleFS not mounted.");
    }
}

void SysInfoShell::cmdFSShell() {
    if (!littleFsService.mounted()) {
        terminalView.println("LittleFS not mounted.");
        return;
    }

    std::vector<std::string> actions = {
        " 📁 List files",
        " 🧹 Delete file",
        " 💣 Delete ALL",
        " 🚪 Exit"
    };

    bool loop = true;
    while (loop) {
        terminalView.println("\n=== LittleFS Manager ===");
        int choice = userInputManager.readValidatedChoiceIndex("Select action", actions, actions.size() - 1);

        switch (choice) {
            case 0: fsListFiles();  break;
            case 1: fsDeleteFile(); break;
            case 2: fsDeleteAll();  break;
            case 3: // Exit
            default:
                loop = false;
                break;
        }
    }

    terminalView.println("Exiting LittleFS...");
}

void SysInfoShell::cmdNVS() {
    terminalView.println("\n=== Non Volatile Storage ===");
    terminalView.println(systemService.getNvsStats());
    terminalView.println(systemService.getNvsEntries());
}

void SysInfoShell::cmdNet() {
    terminalView.println("\n=== Network Info ===");

    auto ssid     = wifiService.getSsid();     if (ssid.empty()) ssid = "N/A";
    auto bssid    = wifiService.getBssid();    if (bssid.empty()) bssid = "N/A";
    auto hostname = wifiService.getHostname(); if (hostname.empty()) hostname = "N/A";

    terminalView.println("Base MAC     : " + systemService.getBaseMac());
    terminalView.println("AP MAC       : " + wifiService.getMacAddressAp());
    terminalView.println("STA MAC      : " + wifiService.getMacAddressSta());
    terminalView.println("IP           : " + wifiService.getLocalIp());
    terminalView.println("Subnet       : " + wifiService.getSubnetMask());
    terminalView.println("Gateway      : " + wifiService.getGatewayIp());
    terminalView.println("DNS1         : " + wifiService.getDns1());
    terminalView.println("DNS2         : " + wifiService.getDns2());
    terminalView.println("Hostname     : " + hostname);

    terminalView.println("SSID         : " + ssid);
    terminalView.println("BSSID        : " + bssid);

    const int status = wifiService.getWifiStatusRaw();
    if (status == 3 /* WL_CONNECTED */) {
        terminalView.println("RSSI         : " + std::to_string(wifiService.getRssi()) + " dBm");
        terminalView.println("Channel      : " + std::to_string(wifiService.getChannel()));
    } else {
        terminalView.println("RSSI         : N/A");
        terminalView.println("Channel      : N/A");
    }

    terminalView.println("Mode         : " + std::string(wifiService.wifiModeToStr(wifiService.getWifiModeRaw())));
    terminalView.println("Status       : " + std::string(wifiService.wlStatusToStr(status)));
    terminalView.println("Prov enabled : " + std::string(wifiService.isProvisioningEnabled() ? "Yes" : "No"));
}

void SysInfoShell::cmdReboot(bool hard) {
    auto confirmation = userInputManager.readYesNo("Reboot the device? (y/n)", false);
    if (confirmation) {
        terminalView.println("\nRebooting, your session will be lost...");
        systemService.reboot(hard);
    }
}

void SysInfoShell::cmdUsbBootloaderMode() {
    terminalView.println("\n=== USB Bootloader Mode ===");
    terminalView.println("This mode restarts the ESP32 into USB BOOT MODE.");
    terminalView.println("Use it when you want to flash the device firmware.\n");

    auto confirmation = userInputManager.readYesNo("Reboot into USB bootloader mode? (y/n)", false);
    if (confirmation) {
        terminalView.println("\nRebooting into USB bootloader mode, the terminal will close...");
        deviceView.clear();
        deviceView.topBar("USB BOOT MODE", false, false);
        systemService.rebootToBootloader();
    }
}

void SysInfoShell::cmdDebugLogs() {
    terminalView.println("\n=== Debug Logs ===");
    terminalView.println("This will enable debug output to serial,");
    terminalView.println("allow you to see the ESP debug logs.\n");

    auto confirm = userInputManager.readYesNo("Enable debug output to serial?", false);
    if (!confirm) return;
    systemService.setDebugOutput(true);
    terminalView.println("Debug output enabled. Logs will now be printed to serial.");
}

void SysInfoShell::fsListFiles() {
    const auto entries = littleFsService.list("/");

    std::vector<LittleFsService::Entry> files;
    files.reserve(entries.size());
    for (const auto& e : entries) {
        if (!e.isDir) files.push_back(e);
    }

    if (files.empty()) {
        terminalView.println("\nNo files in LittleFS.");
        return;
    }

    terminalView.println("\n=== LittleFS Files ===");
    for (size_t i = 0; i < files.size(); ++i) {
        auto sizeStr = files[i].size > 1024
            ? (std::to_string(files[i].size / 1024) + " KB")
            : (std::to_string(files[i].size) + " B");

        terminalView.println(" - " + files[i].name + " (" + sizeStr + ")");
    }
}

void SysInfoShell::fsDeleteFile() {
    const auto entries = littleFsService.list("/");

    std::vector<LittleFsService::Entry> files;
    files.reserve(entries.size());
    for (const auto& e : entries) {
        if (!e.isDir) files.push_back(e);
    }

    if (files.empty()) {
        terminalView.println("\nNo files to delete.");
        return;
    }

    std::vector<std::string> choices;
    choices.reserve(files.size() + 1);

    terminalView.println("\n=== LittleFS Delete ===");
    for (size_t i = 0; i < files.size(); ++i) {
        auto sizeStr = files[i].size > 1024
            ? (std::to_string(files[i].size / 1024) + " KB")
            : (std::to_string(files[i].size) + " B");

        choices.emplace_back(" " + files[i].name + " (" + sizeStr + ")");
    }
    choices.emplace_back(" Exit");

    int idx = userInputManager.readValidatedChoiceIndex("Select file to delete", choices, choices.size() - 1);

    if (idx == (int)choices.size() - 1) {
        terminalView.println("LittleFS: Exiting delete menu...");
        return;
    }

    // Path normalize
    std::string path = files[(size_t)idx].name;
    if (!path.empty() && path[0] != '/') {
        path.insert(path.begin(), '/');
    }

    const std::string prompt = "Delete '" + path + "' ?";
    bool ok = userInputManager.readYesNo(prompt.c_str(), false);
    if (!ok) {
        terminalView.println("Delete cancelled.");
        return;
    }

    if (!littleFsService.exists(path)) {
        terminalView.println("File does not exist anymore.");
        return;
    }

    if (!littleFsService.removeFile(path)) {
        terminalView.println("Delete failed.");
        return;
    }

    terminalView.println("Deleted: " + path);
}

void SysInfoShell::fsDeleteAll() {
    const auto entries = littleFsService.list("/");

    std::vector<LittleFsService::Entry> files;
    files.reserve(entries.size());
    for (const auto& e : entries) {
        if (!e.isDir) files.push_back(e);
    }

    if (files.empty()) {
        terminalView.println("\nNo files to delete.");
        return;
    }

    terminalView.println("\n[⚠️  WARNING]");
    terminalView.println(" This will delete ALL files in LittleFS.");
    terminalView.println(" Type 'DELETE ALL' to confirm.\n");

    std::string confirm = userInputManager.readString("Confirm", "");
    if (confirm != "DELETE ALL") {
        terminalView.println("Delete cancelled.");
        return;
    }

    size_t deleted = 0;
    size_t failed  = 0;

    terminalView.println("Deleting files...");
    for (auto& f : files) {
        std::string path = f.name;
        if (!path.empty() && path[0] != '/') {
            path.insert(path.begin(), '/');
        }

        if (littleFsService.exists(path) && littleFsService.removeFile(path)) {
            deleted++;
        } else {
            failed++;
        }
    }

    terminalView.println("Delete ALL complete.");
    terminalView.println("Deleted: " + std::to_string(deleted));
    if (failed > 0) {
        terminalView.println("Failed : " + std::to_string(failed));
    }
}
