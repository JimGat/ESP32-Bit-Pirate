#include "NvsService.h"

NvsService::~NvsService() {
    preferences.end(); // Close namespace
}

void NvsService::open() {
    // Open nvs namespace
    preferences.begin(globalState.getNvsNamespace(), false);
}

void NvsService::close() {
    preferences.end(); // Close namespace
}

bool NvsService::hasKey(const std::string& key) {
    return preferences.isKey(key.c_str());
}

void NvsService::saveString(const std::string& key, const std::string& value) {
    preferences.putString(key.c_str(), value.c_str());
}

std::string NvsService::getString(const std::string& key, const std::string& defaultValue) {
    if (hasKey(key)) {
        return preferences.getString(key.c_str(), defaultValue.c_str()).c_str();
    }
    return defaultValue;
}

void NvsService::saveInt(const std::string& key, int value) {
    preferences.putInt(key.c_str(), value);
}

int NvsService::getInt(const std::string& key, int defaultValue) {
    return preferences.getInt(key.c_str(), defaultValue);
}

void NvsService::remove(const std::string& key) {
    preferences.remove(key.c_str());
}

void NvsService::clearNamespace() {
    preferences.clear();
}

void NvsService::saveOneShotBootMode(OneShotBootMode mode) {
    preferences.putUChar("oneshot_boot", static_cast<uint8_t>(mode));
}

OneShotBootMode NvsService::getOneShotBootMode() {
    return static_cast<OneShotBootMode>(
        preferences.getUChar("oneshot_boot", static_cast<uint8_t>(OneShotBootMode::None))
    );
}

void NvsService::clearOneShotBootMode() {
    preferences.remove("oneshot_boot");
}

OneShotBootMode NvsService::consumeOneShotBootMode() {
    OneShotBootMode mode = getOneShotBootMode();
    if (mode != OneShotBootMode::None) {
        clearOneShotBootMode();
    }
    return mode;
}

void NvsService::saveOneShotUsbUartBridgeConfig(uint8_t rxPin, uint8_t txPin, bool inverted) {
    preferences.putUChar("oneshot_uart_rx", rxPin);
    preferences.putUChar("oneshot_uart_tx", txPin);
    preferences.putBool("oneshot_uart_inv", inverted);
}

void NvsService::getOneShotUsbUartBridgeConfig(uint8_t defaultRxPin, uint8_t defaultTxPin, bool defaultInverted, uint8_t& rxPin, uint8_t& txPin, bool& inverted) {
    rxPin = preferences.getUChar("oneshot_uart_rx", defaultRxPin);
    txPin = preferences.getUChar("oneshot_uart_tx", defaultTxPin);
    inverted = preferences.getBool("oneshot_uart_inv", defaultInverted);
}

void NvsService::clearOneShotUsbUartBridgeConfig() {
    preferences.remove("oneshot_uart_rx");
    preferences.remove("oneshot_uart_tx");
    preferences.remove("oneshot_uart_inv");
}

void NvsService::saveOneShotFlashromSerprogConfig(uint8_t csPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin, uint32_t frequency) {
    preferences.putUChar("oneshot_fr_cs", csPin);
    preferences.putUChar("oneshot_fr_sck", sckPin);
    preferences.putUChar("oneshot_fr_miso", misoPin);
    preferences.putUChar("oneshot_fr_mosi", mosiPin);
    preferences.putUInt("oneshot_fr_freq", frequency);
}

void NvsService::getOneShotFlashromSerprogConfig(uint8_t defaultCsPin, uint8_t defaultSckPin, uint8_t defaultMisoPin, uint8_t defaultMosiPin, uint32_t defaultFrequency, uint8_t& csPin, uint8_t& sckPin, uint8_t& misoPin, uint8_t& mosiPin, uint32_t& frequency) {
    csPin = preferences.getUChar("oneshot_fr_cs", defaultCsPin);
    sckPin = preferences.getUChar("oneshot_fr_sck", defaultSckPin);
    misoPin = preferences.getUChar("oneshot_fr_miso", defaultMisoPin);
    mosiPin = preferences.getUChar("oneshot_fr_mosi", defaultMosiPin);
    frequency = preferences.getUInt("oneshot_fr_freq", defaultFrequency);
}

void NvsService::clearOneShotFlashromSerprogConfig() {
    preferences.remove("oneshot_fr_cs");
    preferences.remove("oneshot_fr_sck");
    preferences.remove("oneshot_fr_miso");
    preferences.remove("oneshot_fr_mosi");
    preferences.remove("oneshot_fr_freq");
}

void NvsService::saveOneShotSumpLogicAnalyzerConfig(const uint8_t* pins, uint8_t channelCount) {
    preferences.putUChar("oneshot_la_count", channelCount);

    for (uint8_t i = 0; i < 8; ++i) {
        std::string key = "oneshot_la_pin" + std::to_string(i);
        preferences.putUChar(key.c_str(), pins[i]);
    }
}

void NvsService::getOneShotSumpLogicAnalyzerConfig(uint8_t* pins, uint8_t defaultChannelCount, uint8_t& channelCount) {
    channelCount = preferences.getUChar("oneshot_la_count", defaultChannelCount);

    for (uint8_t i = 0; i < 8; ++i) {
        std::string key = "oneshot_la_pin" + std::to_string(i);
        pins[i] = preferences.getUChar(key.c_str(), pins[i]);
    }
}

void NvsService::clearOneShotSumpLogicAnalyzerConfig() {
    preferences.remove("oneshot_la_count");
    preferences.remove("oneshot_la_rate");
    preferences.remove("oneshot_la_samples");

    for (uint8_t i = 0; i < 8; ++i) {
        std::string key = "oneshot_la_pin" + std::to_string(i);
        preferences.remove(key.c_str());
    }
}

void NvsService::saveOneShotOpenOcdBusPirateConfig(uint8_t tckPin, uint8_t tmsPin, uint8_t tdiPin, uint8_t tdoPin, uint8_t swclkPin, uint8_t swdioPin) {
    preferences.putUChar("oneshot_ocd_tck", tckPin);
    preferences.putUChar("oneshot_ocd_tms", tmsPin);
    preferences.putUChar("oneshot_ocd_tdi", tdiPin);
    preferences.putUChar("oneshot_ocd_tdo", tdoPin);
    preferences.putUChar("oneshot_ocd_swclk", swclkPin);
    preferences.putUChar("oneshot_ocd_swdio", swdioPin);
}

void NvsService::getOneShotOpenOcdBusPirateConfig(uint8_t defaultTckPin, uint8_t defaultTmsPin, uint8_t defaultTdiPin, uint8_t defaultTdoPin, uint8_t defaultSwclkPin, uint8_t defaultSwdioPin, uint8_t& tckPin, uint8_t& tmsPin, uint8_t& tdiPin, uint8_t& tdoPin, uint8_t& swclkPin, uint8_t& swdioPin) {
    tckPin = preferences.getUChar("oneshot_ocd_tck", defaultTckPin);
    tmsPin = preferences.getUChar("oneshot_ocd_tms", defaultTmsPin);
    tdiPin = preferences.getUChar("oneshot_ocd_tdi", defaultTdiPin);
    tdoPin = preferences.getUChar("oneshot_ocd_tdo", defaultTdoPin);
    swclkPin = preferences.getUChar("oneshot_ocd_swclk", defaultSwclkPin);
    swdioPin = preferences.getUChar("oneshot_ocd_swdio", defaultSwdioPin);
}

void NvsService::clearOneShotOpenOcdBusPirateConfig() {
    preferences.remove("oneshot_ocd_tck");
    preferences.remove("oneshot_ocd_tms");
    preferences.remove("oneshot_ocd_tdi");
    preferences.remove("oneshot_ocd_tdo");
    preferences.remove("oneshot_ocd_swclk");
    preferences.remove("oneshot_ocd_swdio");
}

void NvsService::saveOneShotAvrDudeBusPirateConfig(uint8_t csPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin, uint32_t frequency) {
    preferences.putUChar("oneshot_avr_cs", csPin);
    preferences.putUChar("oneshot_avr_sck", sckPin);
    preferences.putUChar("oneshot_avr_miso", misoPin);
    preferences.putUChar("oneshot_avr_mosi", mosiPin);
    preferences.putUInt("oneshot_avr_freq", frequency);
}

void NvsService::getOneShotAvrDudeBusPirateConfig(uint8_t defaultCsPin, uint8_t defaultSckPin, uint8_t defaultMisoPin, uint8_t defaultMosiPin, uint32_t defaultFrequency, uint8_t& csPin, uint8_t& sckPin, uint8_t& misoPin, uint8_t& mosiPin, uint32_t& frequency) {
    csPin = preferences.getUChar("oneshot_avr_cs", defaultCsPin);
    sckPin = preferences.getUChar("oneshot_avr_sck", defaultSckPin);
    misoPin = preferences.getUChar("oneshot_avr_miso", defaultMisoPin);
    mosiPin = preferences.getUChar("oneshot_avr_mosi", defaultMosiPin);
    frequency = preferences.getUInt("oneshot_avr_freq", defaultFrequency);
}

void NvsService::clearOneShotAvrDudeBusPirateConfig() {
    preferences.remove("oneshot_avr_cs");
    preferences.remove("oneshot_avr_sck");
    preferences.remove("oneshot_avr_miso");
    preferences.remove("oneshot_avr_mosi");
    preferences.remove("oneshot_avr_freq");
}
