#include "ThreeWireEepromShell.h"

ThreeWireEepromShell::ThreeWireEepromShell(
    ITerminalView& terminalView,
    IInput& terminalInput,
    UserInputManager& userInputManager,
    ThreeWireService& threeWireService,
    ArgTransformer& argTransformer)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      userInputManager(userInputManager),
      threeWireService(threeWireService),
      argTransformer(argTransformer) {}

void ThreeWireEepromShell::run() {

    const std::vector<std::string> actions = {
        "🔍 Probe",
        "📖 Read bytes",
        "✏️  Write bytes",
        "🗃️  Dump EEPROM",
        "💣 Erase EEPROM",
        "🚪 Exit Shell"
    };

    // EEPROM Model
    std::vector<std::string> modelOptions = threeWireService.getSupportedModels();
    int modelIndex = userInputManager.readValidatedChoiceIndex("\nSelect EEPROM model", modelOptions, state.getThreeWireEepromModelIndex());
    int modelId = threeWireService.resolveModelId(modelOptions[modelIndex]);
    terminalView.println("\n✅ Selected model: " + modelOptions[modelIndex] + " (ID: " + std::to_string(modelId) + ")");
    state.setThreeWireEepromModelIndex(modelIndex);

    // Organization
    terminalView.println("\n⚠️  ORG is a physical pin on the EEPROM chip.");
    terminalView.println("   Tie it to GND for 8-bit (x8) organization.");
    terminalView.println("   Tie it to VCC for 16-bit (x16) organization.");
    terminalView.println("   This applies to chips with configurable ORG pins (most of them).");
    terminalView.println("   Fixed organization chips:");
    terminalView.println("     • 93xx56A → always 8-bit");
    terminalView.println("     • 93xx56B → always 16-bit\n");
    bool org8 = userInputManager.readYesNo("EEPROM organization 8 bits ?", false);
    state.setThreeWireOrg8(org8);
    
    auto cs = state.getThreeWireCsPin();
    auto sk = state.getThreeWireSkPin();
    auto di = state.getThreeWireDiPin();
    auto doPin = state.getThreeWireDoPin();
    threeWireService.end();
    threeWireService.configure(cs, sk, di, doPin, modelId, org8);
    
    while (true) {
        // Select action
        terminalView.println("\n=== 3WIRE EEPROM Shell ===");
        int index = userInputManager.readValidatedChoiceIndex("Select EEPROM action", actions, 0);

        // Quit
        if (index == -1 || actions[index] == "🚪 Exit Shell") {
            terminalView.println("Exiting EEPROM shell...\n");
            break;
        }

        // Dispatch
        switch (index) {
            case 0: cmdProbe(); break;
            case 1: cmdRead(); break;
            case 2: cmdWrite(); break;
            case 3: cmdDump(); break;
            case 4: cmdErase(); break;
        }
    }
}

/*
EEPROM Probe
*/
void ThreeWireEepromShell::cmdProbe() {
    bool isOrg8 = state.isThreeWireOrg8();
    bool isBlank = true;

    if (isOrg8) {
        std::vector<uint8_t> data = threeWireService.dump8();
        for (uint8_t val : data) {
            if (val != 0xFF) {
                isBlank = false;
                break;
            }
        }
    } else {
        std::vector<uint16_t> data = threeWireService.dump16();
        for (uint16_t val : data) {
            if (val != 0xFFFF) {
                isBlank = false;
                break;
            }
        }
    }

    if (!isBlank) {
        terminalView.println("\n3WIRE EEPROM: Detected ✅\n");
    } else {
        terminalView.println("\n3WIRE EEPROM: No EEPROM detected or EEPROM is blank ❌\n");
    }
}

/*
EEPROM Read
*/
void ThreeWireEepromShell::cmdRead() {
    auto addr = userInputManager.readValidatedUint16("Start address (dec or 0x hex)", 0, true);
    uint16_t count = userInputManager.readValidatedUint16("Number of bytes to read (dec or 0x hex)", 16, true);
    bool isOrg8 = state.isThreeWireOrg8();
    uint16_t eepromSize = threeWireService.sizeBytes();
    uint32_t startByte = isOrg8 ? addr : static_cast<uint32_t>(addr) * 2;

    if (startByte >= eepromSize) {
        terminalView.println("\n❌ Error: Start address is beyond EEPROM size.\n");
        return;
    }

    if (startByte + count > eepromSize) {
        count = eepromSize - startByte;
    }

    terminalView.println("");
    if (count == 1 && isOrg8) {
        uint8_t val = threeWireService.read8(addr);
        terminalView.println("✅ 3WIRE EEPROM: Read 0x" + argTransformer.toHex(addr, 4) +
                             " = 0x" + argTransformer.toHex(val, 2));
    } else {
        if (isOrg8) {
            std::vector<uint8_t> values;
            for (uint16_t i = 0; i < count; ++i) {
                values.push_back(threeWireService.read8(addr + i));
            }
            for (size_t i = 0; i < values.size(); i += 16) {
                uint32_t displayAddr = addr + i;
                size_t chunkSize = std::min<size_t>(16, values.size() - i);
                std::vector<uint8_t> chunk(values.begin() + i, values.begin() + i + chunkSize);
                terminalView.println(argTransformer.toAsciiLine(displayAddr, chunk));
            }
        } else {
            std::vector<uint8_t> values;
            uint16_t wordCount = (count + 1) / 2;
            values.reserve(count);
            for (uint16_t i = 0; i < wordCount; ++i) {
                uint16_t word = threeWireService.read16(addr + i);
                values.push_back((word >> 8) & 0xFF);
                if (values.size() < count) {
                    values.push_back(word & 0xFF);
                }
            }
            for (size_t i = 0; i < values.size(); i += 16) {
                uint32_t displayAddr = startByte + i;
                size_t chunkSize = std::min<size_t>(16, values.size() - i);
                std::vector<uint8_t> chunk(values.begin() + i, values.begin() + i + chunkSize);
                terminalView.println(argTransformer.toAsciiLine(displayAddr, chunk));
            }
        }
    }
    terminalView.println("");
}

/*
EEPROM Write
*/
void ThreeWireEepromShell::cmdWrite() {
    auto addr = userInputManager.readValidatedUint16("Start address (dec or 0x hex)", 0, true);
    auto hexStr = userInputManager.readValidatedHexString("Enter byte values (e.g., 01 A5 FF...) ", 0, true);
    auto data = argTransformer.parseHexList(hexStr);

    bool isOrg8 = state.isThreeWireOrg8();
    threeWireService.writeEnable();

    terminalView.println("");
    for (size_t i = 0; i < data.size(); ++i) {
        if (isOrg8) {
            threeWireService.write8(addr + i, data[i]);
            terminalView.println("3WIRE EEPROM: Write 0x" + argTransformer.toHex(addr + i, 4) +
                                    " = 0x" + argTransformer.toHex(data[i], 2) + " ✅");
        } else {
            if (i + 1 >= data.size()) break; // Incomplet
            uint16_t val = (data[i] << 8) | data[i + 1];
            threeWireService.write16(addr + (i / 2), val);
            terminalView.println("3WIRE EEPROM: Write 0x" + argTransformer.toHex(addr + (i / 2), 4) +
                                    " = 0x" + argTransformer.toHex(val, 4) + " ✅");
            ++i; // Consomme 2 bytes
        
        }
    }
    terminalView.println("");

    threeWireService.writeDisable();
}

/*
EEPROM Dump
*/
void ThreeWireEepromShell::cmdDump() {
    bool isOrg8 = state.isThreeWireOrg8();
    uint16_t start = 0;

    terminalView.println("");
    if (isOrg8) {
        auto data = threeWireService.dump8();
        for (size_t i = start; i < data.size(); i += 16) {
            uint32_t addr = i;
            size_t chunkSize = std::min<size_t>(16, data.size() - i);
            std::vector<uint8_t> chunk(data.begin() + i, data.begin() + i + chunkSize);
            terminalView.println(argTransformer.toAsciiLine(addr, chunk));
        }
    } else {
        auto data = threeWireService.dump16();
        for (size_t i = start; i < data.size(); i += 8) {
            uint32_t addr = i * 2;
            size_t chunkSize = std::min<size_t>(8, data.size() - i);
            std::vector<uint16_t> chunk(data.begin() + i, data.begin() + i + chunkSize);
            terminalView.println(argTransformer.toAsciiLine(addr, chunk));
        }
    }
    terminalView.println("");
}

/*
EEPROM Erase
*/
void ThreeWireEepromShell::cmdErase() {
    
    auto confirmation = userInputManager.readYesNo("Are you sure you want to erase the EEPROM?", false);
    if (!confirmation) {
        terminalView.println("\n3WIRE EEPROM: ❌ Erase cancelled.\n");
        return;
    }

    threeWireService.writeEnable();
    threeWireService.eraseAll();
    threeWireService.writeDisable();
    bool isOrg8 = state.isThreeWireOrg8();
    bool success = true;

    if (isOrg8) {
        auto data = threeWireService.dump8();
        for (uint8_t val : data) {
            if (val != 0xFF) {
                success = false;
                break;
            }
        }
    } else {
        auto data = threeWireService.dump16();
        for (uint16_t val : data) {
            if (val != 0xFFFF) {
                success = false;
                break;
            }
        }
    }

    if (success) {
        terminalView.println("\n3WIRE EEPROM: ✅ Successfully erased.\n");
    } else {
        terminalView.println("\n3WIRE EEPROM: ❌ Erase verification failed.\n");
    }
}
