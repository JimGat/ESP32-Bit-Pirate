#include "ProfileTransformer.h"

std::string ProfileTransformer::toProfileText() {
    auto& gs = GlobalState::getInstance();
    std::ostringstream o;

    o << "# ESP32 Bus Pirate profile\n";
    o << "version=" << gs.getVersion() << "\n\n";

    o << "spi.cs=" << (int)gs.getSpiCSPin() << "\n";
    o << "spi.clk=" << (int)gs.getSpiCLKPin() << "\n";
    o << "spi.miso=" << (int)gs.getSpiMISOPin() << "\n";
    o << "spi.mosi=" << (int)gs.getSpiMOSIPin() << "\n";
    o << "spi.freq=" << gs.getSpiFrequency() << "\n\n";

    o << "i2c.sda=" << (int)gs.getI2cSdaPin() << "\n";
    o << "i2c.scl=" << (int)gs.getI2cSclPin() << "\n";
    o << "i2c.freq=" << gs.getI2cFrequency() << "\n\n";

    o << "uart.baud=" << gs.getUartBaudRate() << "\n";
    o << "uart.rx=" << (int)gs.getUartRxPin() << "\n";
    o << "uart.tx=" << (int)gs.getUartTxPin() << "\n";
    o << "uart.databits=" << (int)gs.getUartDataBits() << "\n";
    o << "uart.parity=" << gs.getUartParity() << "\n";
    o << "uart.stopbits=" << (int)gs.getUartStopBits() << "\n";
    o << "uart.inverted=" << (gs.isUartInverted() ? 1 : 0) << "\n\n";

    o << "ir.tx=" << (int)gs.getInfraredTxPin() << "\n";
    o << "ir.rx=" << (int)gs.getInfraredRxPin() << "\n";
    o << "ir.protocol=" << InfraredProtocolMapper::toString(gs.getInfraredProtocol()) << "\n\n";

    o << "led.data=" << (int)gs.getLedDataPin() << "\n";
    o << "led.clock=" << (int)gs.getLedClockPin() << "\n";
    o << "led.len=" << gs.getLedLength() << "\n";
    o << "led.proto=" << gs.getLedProtocol() << "\n";
    o << "led.brightness=" << (int)gs.getLedBrightness() << "\n";

    // HDUART
    o << "hduart.baud=" << gs.getHdUartBaudRate() << "\n";
    o << "hduart.pin=" << (int)gs.getHdUartPin() << "\n";
    o << "hduart.databits=" << (int)gs.getHdUartDataBits() << "\n";
    o << "hduart.parity=" << gs.getHdUartParity() << "\n";
    o << "hduart.stopbits=" << (int)gs.getHdUartStopBits() << "\n";
    o << "hduart.inverted=" << (gs.isHdUartInverted() ? 1 : 0) << "\n\n";

    // I2S
    o << "i2s.bclk=" << (int)gs.getI2sBclkPin() << "\n";
    o << "i2s.lrck=" << (int)gs.getI2sLrckPin() << "\n";
    o << "i2s.data=" << (int)gs.getI2sDataPin() << "\n";
    o << "i2s.rate=" << gs.getI2sSampleRate() << "\n";
    o << "i2s.bits=" << (int)gs.getI2sBitsPerSample() << "\n\n";

    // CAN
    o << "can.cs=" << (int)gs.getCanCspin() << "\n";
    o << "can.sck=" << (int)gs.getCanSckPin() << "\n";
    o << "can.si=" << (int)gs.getCanSiPin() << "\n";
    o << "can.so=" << (int)gs.getCanSoPin() << "\n";
    o << "can.kbps=" << gs.getCanKbps() << "\n\n";

    // SubGHz
    o << "subghz.sck=" << (int)gs.getSubGhzSckPin() << "\n";
    o << "subghz.miso=" << (int)gs.getSubGhzMisoPin() << "\n";
    o << "subghz.mosi=" << (int)gs.getSubGhzMosiPin() << "\n";
    o << "subghz.cs=" << (int)gs.getSubGhzCsPin() << "\n";
    o << "subghz.gdo=" << (int)gs.getSubGhzGdoPin() << "\n";
    o << "subghz.freq=" << gs.getSubGhzFrequency() << "\n";

    // NRF24
    o << "rf24.csn=" << (int)gs.getRf24CsnPin() << "\n";
    o << "rf24.ce=" << (int)gs.getRf24CePin() << "\n";
    o << "rf24.sck=" << (int)gs.getRf24SckPin() << "\n";
    o << "rf24.miso=" << (int)gs.getRf24MisoPin() << "\n";
    o << "rf24.mosi=" << (int)gs.getRf24MosiPin() << "\n\n";

    // RFID
    o << "rfid.sda=" << (int)gs.getRfidSdaPin() << "\n";
    o << "rfid.scl=" << (int)gs.getRfidSclPin() << "\n\n";

    // SD
    o << "sd.cs=" << (int)gs.getSdCardCsPin() << "\n";
    o << "sd.clk=" << (int)gs.getSdCardClkPin() << "\n";
    o << "sd.miso=" << (int)gs.getSdCardMisoPin() << "\n";
    o << "sd.mosi=" << (int)gs.getSdCardMosiPin() << "\n";
    o << "sd.freq=" << gs.getSdCardFrequency() << "\n";


    return o.str();
}

static bool getStr(const std::map<std::string,std::string>& kv,
                   const char* key,
                   std::string& out) {
    auto it = kv.find(key);
    if (it == kv.end()) return false;
    out = it->second;
    return true;
}

static bool parseU32(const std::string& s, uint32_t& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    unsigned long v = std::strtoul(s.c_str(), &end, 10);
    if (!end || *end != '\0') return false;
    out = (uint32_t)v;
    return true;
}

static bool parseU16(const std::string& s, uint16_t& out) {
    uint32_t v = 0;
    if (!parseU32(s, v) || v > 0xFFFF) return false;
    out = (uint16_t)v;
    return true;
}

static bool parseU8(const std::string& s, uint8_t& out) {
    uint32_t v = 0;
    if (!parseU32(s, v) || v > 0xFF) return false;
    out = (uint8_t)v;
    return true;
}

static bool parseBool01(const std::string& s, bool& out) {
    if (s == "1") { out = true; return true; }
    if (s == "0") { out = false; return true; }
    return false;
}

static bool parseF32(const std::string& s, float& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtof(s.c_str(), &end);
    if (!end || *end != '\0') return false;
    return true;
}

bool ProfileTransformer::fromProfileText(
    const std::string& text,
    std::string& error
) {
    error.clear();

    auto& gs = GlobalState::getInstance();
    const auto kv = parse(text);

    // local
    std::string v;
    uint8_t  u8  = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    bool     b   = false;
    float    f32 = 0.0f;

    // -----------------------------
    // SPI 
    // -----------------------------
    if (getStr(kv, "spi.cs", v)   && !parseU8(v, u8))  { error = "Invalid spi.cs"; return false; }
    else if (!v.empty()) gs.setSpiCSPin(u8);

    if (getStr(kv, "spi.clk", v)  && !parseU8(v, u8))  { error = "Invalid spi.clk"; return false; }
    else if (!v.empty()) gs.setSpiCLKPin(u8);

    if (getStr(kv, "spi.miso", v) && !parseU8(v, u8))  { error = "Invalid spi.miso"; return false; }
    else if (!v.empty()) gs.setSpiMISOPin(u8);

    if (getStr(kv, "spi.mosi", v) && !parseU8(v, u8))  { error = "Invalid spi.mosi"; return false; }
    else if (!v.empty()) gs.setSpiMOSIPin(u8);

    if (getStr(kv, "spi.freq", v) && !parseU32(v, u32)) { error = "Invalid spi.freq"; return false; }
    else if (!v.empty()) gs.setSpiFrequency(u32);

    // -----------------------------
    // I2C
    // -----------------------------
    if (getStr(kv, "i2c.sda", v) && !parseU8(v, u8))   { error = "Invalid i2c.sda"; return false; }
    else if (!v.empty()) gs.setI2cSdaPin(u8);

    if (getStr(kv, "i2c.scl", v) && !parseU8(v, u8))   { error = "Invalid i2c.scl"; return false; }
    else if (!v.empty()) gs.setI2cSclPin(u8);

    if (getStr(kv, "i2c.freq", v) && !parseU32(v, u32)) { error = "Invalid i2c.freq"; return false; }
    else if (!v.empty()) gs.setI2cFrequency(u32);

    // -----------------------------
    // UART 
    // -----------------------------
    if (getStr(kv, "uart.baud", v) && !parseU32(v, u32)) { error = "Invalid uart.baud"; return false; }
    else if (!v.empty()) gs.setUartBaudRate((unsigned long)u32);

    if (getStr(kv, "uart.rx", v) && !parseU8(v, u8)) { error = "Invalid uart.rx"; return false; }
    else if (!v.empty()) gs.setUartRxPin(u8);

    if (getStr(kv, "uart.tx", v) && !parseU8(v, u8)) { error = "Invalid uart.tx"; return false; }
    else if (!v.empty()) gs.setUartTxPin(u8);

    if (getStr(kv, "uart.databits", v) && !parseU8(v, u8)) { error = "Invalid uart.databits"; return false; }
    else if (!v.empty()) gs.setUartDataBits(u8);

    if (getStr(kv, "uart.parity", v)) {
        gs.setUartParity(v);
    }

    if (getStr(kv, "uart.stopbits", v) && !parseU8(v, u8)) { error = "Invalid uart.stopbits"; return false; }
    else if (!v.empty()) gs.setUartStopBits(u8);

    if (getStr(kv, "uart.inverted", v) && !parseBool01(v, b)) { error = "Invalid uart.inverted"; return false; }
    else if (!v.empty()) gs.setUartInverted(b);

    // -----------------------------
    // Infrared
    // -----------------------------
    if (getStr(kv, "ir.tx", v) && !parseU8(v, u8)) { error = "Invalid ir.tx"; return false; }
    else if (!v.empty()) gs.setInfraredTxPin(u8);

    if (getStr(kv, "ir.rx", v) && !parseU8(v, u8)) { error = "Invalid ir.rx"; return false; }
    else if (!v.empty()) gs.setInfraredRxPin(u8);

    if (getStr(kv, "ir.protocol", v)) {
        gs.setInfraredProtocol(InfraredProtocolMapper::toEnum(v));
    }

    // -----------------------------
    // LED
    // -----------------------------
    if (getStr(kv, "led.data", v) && !parseU8(v, u8)) { error = "Invalid led.data"; return false; }
    else if (!v.empty()) gs.setLedDataPin(u8);

    if (getStr(kv, "led.clock", v) && !parseU8(v, u8)) { error = "Invalid led.clock"; return false; }
    else if (!v.empty()) gs.setLedClockPin(u8);

    if (getStr(kv, "led.len", v) && !parseU16(v, u16)) { error = "Invalid led.len"; return false; }
    else if (!v.empty()) gs.setLedLength(u16);

    if (getStr(kv, "led.proto", v)) {
        gs.setLedProtocol(v);
    }

    if (getStr(kv, "led.brightness", v) && !parseU8(v, u8)) { error = "Invalid led.brightness"; return false; }
    else if (!v.empty()) gs.setLedBrightness(u8);

    // -----------------------------
    // HDUART
    // -----------------------------
    if (getStr(kv, "hduart.baud", v) && !parseU32(v, u32)) { error = "Invalid hduart.baud"; return false; }
    else if (!v.empty()) gs.setHdUartBaudRate((unsigned long)u32);

    if (getStr(kv, "hduart.pin", v) && !parseU8(v, u8)) { error = "Invalid hduart.pin"; return false; }
    else if (!v.empty()) gs.setHdUartPin(u8);

    if (getStr(kv, "hduart.databits", v) && !parseU8(v, u8)) { error = "Invalid hduart.databits"; return false; }
    else if (!v.empty()) gs.setHdUartDataBits(u8);

    if (getStr(kv, "hduart.parity", v)) {
        gs.setHdUartParity(v);
    }

    if (getStr(kv, "hduart.stopbits", v) && !parseU8(v, u8)) { error = "Invalid hduart.stopbits"; return false; }
    else if (!v.empty()) gs.setHdUartStopBits(u8);

    if (getStr(kv, "hduart.inverted", v) && !parseBool01(v, b)) { error = "Invalid hduart.inverted"; return false; }
    else if (!v.empty()) gs.setHdUartInverted(b);

    // -----------------------------
    // I2S
    // -----------------------------
    if (getStr(kv, "i2s.bclk", v) && !parseU8(v, u8)) { error = "Invalid i2s.bclk"; return false; }
    else if (!v.empty()) gs.setI2sBclkPin(u8);

    if (getStr(kv, "i2s.lrck", v) && !parseU8(v, u8)) { error = "Invalid i2s.lrck"; return false; }
    else if (!v.empty()) gs.setI2sLrckPin(u8);

    if (getStr(kv, "i2s.data", v) && !parseU8(v, u8)) { error = "Invalid i2s.data"; return false; }
    else if (!v.empty()) gs.setI2sDataPin(u8);

    if (getStr(kv, "i2s.rate", v) && !parseU32(v, u32)) { error = "Invalid i2s.rate"; return false; }
    else if (!v.empty()) gs.setI2sSampleRate(u32);

    if (getStr(kv, "i2s.bits", v) && !parseU8(v, u8)) { error = "Invalid i2s.bits"; return false; }
    else if (!v.empty()) gs.setI2sBitsPerSample(u8);

    // -----------------------------
    // CAN
    // -----------------------------
    if (getStr(kv, "can.cs", v) && !parseU8(v, u8)) { error = "Invalid can.cs"; return false; }
    else if (!v.empty()) gs.setCanCspin(u8);

    if (getStr(kv, "can.sck", v) && !parseU8(v, u8)) { error = "Invalid can.sck"; return false; }
    else if (!v.empty()) gs.setCanSckPin(u8);

    if (getStr(kv, "can.si", v) && !parseU8(v, u8)) { error = "Invalid can.si"; return false; }
    else if (!v.empty()) gs.setCanSiPin(u8);

    if (getStr(kv, "can.so", v) && !parseU8(v, u8)) { error = "Invalid can.so"; return false; }
    else if (!v.empty()) gs.setCanSoPin(u8);

    if (getStr(kv, "can.kbps", v) && !parseU32(v, u32)) { error = "Invalid can.kbps"; return false; }
    else if (!v.empty()) gs.setCanKbps(u32);

    // -----------------------------
    // SubGHz
    // -----------------------------
    if (getStr(kv, "subghz.sck", v) && !parseU8(v, u8)) { error = "Invalid subghz.sck"; return false; }
    else if (!v.empty()) gs.setSubGhzSckPin(u8);

    if (getStr(kv, "subghz.miso", v) && !parseU8(v, u8)) { error = "Invalid subghz.miso"; return false; }
    else if (!v.empty()) gs.setSubGhzMisoPin(u8);

    if (getStr(kv, "subghz.mosi", v) && !parseU8(v, u8)) { error = "Invalid subghz.mosi"; return false; }
    else if (!v.empty()) gs.setSubGhzMosiPin(u8);

    if (getStr(kv, "subghz.cs", v) && !parseU8(v, u8)) { error = "Invalid subghz.cs"; return false; }
    else if (!v.empty()) gs.setSubGhzCsPin(u8);

    if (getStr(kv, "subghz.gdo", v) && !parseU8(v, u8)) { error = "Invalid subghz.gdo"; return false; }
    else if (!v.empty()) gs.setSubGhzGdoPin(u8);

    if (getStr(kv, "subghz.freq", v) && !parseF32(v, f32)) { error = "Invalid subghz.freq"; return false; }
    else if (!v.empty()) gs.setSubGhzFrequency(f32);

    // -----------------------------
    // NRF24
    // -----------------------------
    if (getStr(kv, "rf24.csn", v) && !parseU8(v, u8)) { error = "Invalid rf24.csn"; return false; }
    else if (!v.empty()) gs.setRf24CsnPin(u8);

    if (getStr(kv, "rf24.ce", v) && !parseU8(v, u8)) { error = "Invalid rf24.ce"; return false; }
    else if (!v.empty()) gs.setRf24CePin(u8);

    if (getStr(kv, "rf24.sck", v) && !parseU8(v, u8)) { error = "Invalid rf24.sck"; return false; }
    else if (!v.empty()) gs.setRf24SckPin(u8);

    if (getStr(kv, "rf24.miso", v) && !parseU8(v, u8)) { error = "Invalid rf24.miso"; return false; }
    else if (!v.empty()) gs.setRf24MisoPin(u8);

    if (getStr(kv, "rf24.mosi", v) && !parseU8(v, u8)) { error = "Invalid rf24.mosi"; return false; }
    else if (!v.empty()) gs.setRf24MosiPin(u8);

    // -----------------------------
    // RFID
    // -----------------------------
    if (getStr(kv, "rfid.sda", v) && !parseU8(v, u8)) { error = "Invalid rfid.sda"; return false; }
    else if (!v.empty()) gs.setRfidSdaPin(u8);

    if (getStr(kv, "rfid.scl", v) && !parseU8(v, u8)) { error = "Invalid rfid.scl"; return false; }
    else if (!v.empty()) gs.setRfidSclPin(u8);

    // -----------------------------
    // SD Card
    // -----------------------------
    if (getStr(kv, "sd.cs", v) && !parseU8(v, u8)) { error = "Invalid sd.cs"; return false; }
    else if (!v.empty()) gs.setSdCardCsPin(u8);

    if (getStr(kv, "sd.clk", v) && !parseU8(v, u8)) { error = "Invalid sd.clk"; return false; }
    else if (!v.empty()) gs.setSdCardClkPin(u8);

    if (getStr(kv, "sd.miso", v) && !parseU8(v, u8)) { error = "Invalid sd.miso"; return false; }
    else if (!v.empty()) gs.setSdCardMisoPin(u8);

    if (getStr(kv, "sd.mosi", v) && !parseU8(v, u8)) { error = "Invalid sd.mosi"; return false; }
    else if (!v.empty()) gs.setSdCardMosiPin(u8);

    if (getStr(kv, "sd.freq", v) && !parseU32(v, u32)) { error = "Invalid sd.freq"; return false; }
    else if (!v.empty()) gs.setSdCardFrequency(u32);

    return true;
}

std::map<std::string, std::string>
ProfileTransformer::parse(const std::string& text) {
    std::map<std::string, std::string> kv;
    std::stringstream ss(text);
    std::string line;

    while (std::getline(ss, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return kv;
}

