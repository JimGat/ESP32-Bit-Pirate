#include "SerialTerminalView.h"

void SerialTerminalView::initialize() {
    hostSerial.begin(baudrate);
    hostSerial.waitReady();
}

void SerialTerminalView::welcome(TerminalTypeEnum& terminalType, std::string& terminalInfos) {

    GlobalState& state = GlobalState::getInstance();
    std::string version = state.getVersion();

    hostSerial.println("    ____  _ _     ____  _           _       ");
    hostSerial.println("   | __ )(_) |_  |  _ \\(_)_ __ __ _| |_ ___ ");
    hostSerial.println("   |  _ \\| | __| | |_) | | '__/ _` | __/ _ \\");
    hostSerial.println("   | |_) | | |_  |  __/| | | | (_| | ||  __/");
    hostSerial.println("   |____/|_|\\__| |_|   |_|_|  \\__,_|\\__\\___|");
    hostSerial.println();
    hostSerial.println("             ESP32 SWISS ARMY KNIFE");
    hostSerial.println();

    std::string versionLine = "     Version " + version + "           Ready to board";
    hostSerial.println(versionLine.c_str());
    hostSerial.println();
    hostSerial.println(" Type 'mode' to start or 'help' for commands");
    hostSerial.println();
}

void SerialTerminalView::print(const std::string& text) {
    hostSerial.print(text.c_str());
}

void SerialTerminalView::print(const uint8_t data) {
    hostSerial.write(data);
}

void SerialTerminalView::println(const std::string& text) {
    hostSerial.println(text.c_str());
}

void SerialTerminalView::printPrompt(const std::string& mode) {
    if (!mode.empty()) {
        hostSerial.print(mode.c_str());
        hostSerial.print("> ");
    } else {
        hostSerial.print("> ");
    }
}

void SerialTerminalView::clear() {
    hostSerial.write(27);  // ESC
    hostSerial.print("[2J"); // erase screen
    hostSerial.write(27);
    hostSerial.print("[H");  // default cursor pos
}

void SerialTerminalView::waitPress() {
    hostSerial.println("\n\n\rPress any key to start...");
}

void SerialTerminalView::setBaudrate(unsigned long baud) {
    baudrate = baud;
}