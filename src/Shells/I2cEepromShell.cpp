#include "I2cEepromShell.h"

I2cEepromShell::I2cEepromShell(
    ITerminalView& view,
    IInput& input,
    I2cService& i2cService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    BinaryAnalyzer& binaryAnalyzer
) : terminalView(view),
    terminalInput(input),
    i2cService(i2cService),
    argTransformer(argTransformer),
    userInputManager(userInputManager),
    binaryAnalyzer(binaryAnalyzer) {}

void I2cEepromShell::run(uint8_t addr) {

    // Select EEPROM model
    int selectedModelIndex = userInputManager.readValidatedChoiceIndex("\nSelect EEPROM type", kModels, kModelsCount, 0);
    
    // Initialize EEPROM
    uint16_t selectedType = memoryLengths[selectedModelIndex];
    if (!i2cService.initEeprom(selectedType, addr)) {
        terminalView.println("\n❌ EEPROM not detected at 0x" + argTransformer.toHex(addr, 2) + ". Aborting.\n");
        return;
    }
    
    // Set variables
    terminalView.println(
        std::string("\n✅ EEPROM initialized: ") + kModels[selectedModelIndex]
    );
    selectedModel = kModels[selectedModelIndex];
    selectedLength = memoryLengths[selectedModelIndex];
    selectedI2cAddress = addr;
    initialized = true;

    while (true) {
        // Select action
        terminalView.println("\n=== I2C EEPROM Shell ===");
        int index = userInputManager.readValidatedChoiceIndex("Select an EEPROM action", kActions, kActionsCount, 0);
        if (index == -1 || kActions[index] == " 🚪 Exit Shell") {
            terminalView.println("Exiting EEPROM shell...\n");
            break;
        }

        // Run selected action
        switch (index) {
            case 0: cmdProbe(); break;
            case 1: cmdAnalyze(); break;
            case 2: cmdRead(); break;
            case 3: cmdWrite(); break;
            case 4: cmdDump(); break;
            case 5: cmdDump(true); break;
            case 6: cmdErase(); break;
        }
    }
}

void I2cEepromShell::cmdProbe() {
    uint32_t length = i2cService.eepromLength();
    uint32_t memSize = i2cService.eepromGetMemorySize();
    uint16_t pageSize = i2cService.eepromPageSize();
    uint8_t addrBytes = i2cService.eepromAddressBytes();
    uint8_t writeTime = i2cService.eepromWriteTimeMs();

    terminalView.println("\n📄 EEPROM Summary:");
    terminalView.println(" • Capacity:     " + std::to_string(length) + " bytes");
    terminalView.println(" • Memory Size:  " + std::to_string(memSize) + " bytes");
    terminalView.println(" • Page Size:    " + std::to_string(pageSize) + " bytes");
    terminalView.println(" • Address Size: " + std::to_string(addrBytes) + " byte(s)");
    terminalView.println(" • Write Time:   " + std::to_string(writeTime) + " ms");
}

void I2cEepromShell::cmdAnalyze() {
    uint32_t eepromSize = i2cService.eepromLength();
    uint32_t start = 0;
    terminalView.println("\n🔍 Analyzing EEPROM content...\n");

    // Analyze the EEPROM content in chuncks
    BinaryAnalyzer::AnalysisResult result = binaryAnalyzer.analyze(
        start,
        eepromSize,
        [&](uint32_t addr, uint8_t* buf, uint32_t len) {
            for (uint32_t i = 0; i < len; ++i)
                buf[i] = i2cService.eepromReadByte(addr + i);
        }
    );

    // Format and print the analysis result
    auto summary = binaryAnalyzer.formatAnalysis(result);
    terminalView.println(summary);

    // Files
    if (!result.foundFiles.empty()) {
        terminalView.println("\n📁 Detected file signatures:");
        for (const auto& file : result.foundFiles) {
            terminalView.println("   - " + file);
        }
    }

    // Secrets
    if (!result.foundSecrets.empty()) {
        terminalView.println("\n🔑 Potential secrets found:");
        for (const auto& secret : result.foundSecrets) {
            terminalView.println("   - " + secret);
        }
    }

}

void I2cEepromShell::cmdRead() {
    auto addrStr = userInputManager.readValidatedHexString("Start address (e.g., 00FF00) ", 0, true);
    auto addr = argTransformer.parseHexOrDec32("0x" + addrStr);
    uint32_t eepromSize = i2cService.eepromLength();
    if (addr >= eepromSize) {
        terminalView.println("\n❌ Error: Start address is beyond EEPROM size.");
        return;
    }
    
    uint32_t count = userInputManager.readValidatedUint32("Number of bytes to read:", 16);
    terminalView.println("");
    if (addr + count > eepromSize) {
        count = eepromSize - addr;
    }

    const uint8_t bytesPerLine = 16;
    for (uint32_t i = 0; i < count; i += bytesPerLine) {
        std::vector<uint8_t> line;
        for (uint8_t j = 0; j < bytesPerLine && (i + j) < count; ++j) {
            line.push_back(i2cService.eepromReadByte(addr + i + j));
        }

        std::string formattedLine = argTransformer.toAsciiLine(addr + i, line);
        terminalView.println(formattedLine);
    }    
}

void I2cEepromShell::cmdWrite() {
    auto addrStr = userInputManager.readValidatedHexString("Start address:", 0, true);
    auto addr = argTransformer.parseHexOrDec32("0x" + addrStr);
    auto hexStr = userInputManager.readValidatedHexString("Enter byte values (e.g., 01 A5 FF...) ", 0, true);
    auto data = argTransformer.parseHexList(hexStr);

    bool ok = true;
    for (size_t i = 0; i < data.size(); ++i) {
        i2cService.eepromWriteByte(addr + i, data[i]);
    }

    terminalView.println("\n✅ Data written.");
}

void I2cEepromShell::cmdDump(bool raw) {
    uint32_t addr = 0;
    uint32_t count = i2cService.eepromLength();

    if (raw) {
        auto confirm = userInputManager.readYesNo("The raw mode is for python scripting, Continue?", false);
        if (!confirm) return;
    }

    const uint8_t bytesPerLine = 16;

    if (raw) {
        // Mode RAW
        for (uint32_t i = 0; i < count; ++i) {
            uint8_t value = i2cService.eepromReadByte(addr + i);
            terminalView.print(value); 
        }
    } else {
        // Mode HEX/ASCII
        terminalView.println("");
        for (uint32_t i = 0; i < count; i += bytesPerLine) {
            std::vector<uint8_t> line;
            for (uint8_t j = 0; j < bytesPerLine && (i + j) < count; ++j) {
                line.push_back(i2cService.eepromReadByte(addr + i + j));

                char c = terminalInput.readChar();
                if (c == '\n' || c == '\r') {
                    terminalView.println("\n❌ Dump interrupted by user.");
                    return;
                }
            }
            std::string formatted = argTransformer.toAsciiLine(addr + i, line);
            terminalView.println(formatted);
        }
    }
}

void I2cEepromShell::cmdErase() {
    bool confirm = userInputManager.readYesNo("⚠️  Are you sure you want to erase the EEPROM?", false);
    if (confirm) {
        terminalView.println("Erasing...");
        i2cService.eepromErase(0xFF);
        terminalView.println("\n✅ EEPROM erased.");
    } else {
        terminalView.println("\n❌ Operation cancelled.");
    }
}