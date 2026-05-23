#include "Rf24Service.h"

bool Rf24Service::configure(
        uint8_t csnPin,
        uint8_t cePin,
        uint8_t sckPin,
        uint8_t misoPin,
        uint8_t mosiPin,
        SPIClass& spi,
        uint32_t spiSpeed
    ) {
    cePin_ = cePin;
    csnPin_ = csnPin;
    sckPin_ = sckPin;
    misoPin_ = misoPin;
    mosiPin_ = mosiPin;
    spiSpeed_ = spiSpeed;

    #ifdef DEVICE_TEMBEDS3CC1101PLUS
        initTembedPlus();
    #endif

    isInitialized = false;

    if (radio_) {
        delete radio_;
        radio_ = nullptr;
    }

    spi.end();
    delay(10);
    spi.begin(sckPin_, misoPin_, mosiPin_, csnPin_);
    
    radio_ = new RF24(cePin_, csnPin_);
    if (!radio_ || !radio_->begin(&spi, sckPin_, misoPin_, mosiPin_, csnPin_)) {
        delete radio_;
        radio_ = nullptr;
        return false;
    }

    isInitialized = true;
    return true;
}

void Rf24Service::initRx() {
    if (!isInitialized) return;
    radio_->setAutoAck(false);
    radio_->setCRCLength(RF24_CRC_DISABLED);
    radio_->setDataRate(RF24_1MBPS);
    radio_->setAddressWidth(2);
}

bool Rf24Service::initRx(const Rf24Config& cfg) {
    if (!isInitialized || radio_ == nullptr) return false;

    // Channel
    radio_->setChannel(cfg.channel);

    // Address len
    radio_->setAddressWidth(cfg.addrLen);

    // CRC
    if (cfg.crcBits == 0) {
        radio_->disableCRC();
    } else if (cfg.crcBits == 8) {
        radio_->setCRCLength(RF24_CRC_8);
    } else {
        radio_->setCRCLength(RF24_CRC_16);
    }

    // Data rate
    if (cfg.dataRate == 0) radio_->setDataRate(RF24_250KBPS);
    else if (cfg.dataRate == 2) radio_->setDataRate(RF24_2MBPS);
    else radio_->setDataRate(RF24_1MBPS);

    // Payload mode
    if (cfg.dynamicPayloads) {
        radio_->enableDynamicPayloads();
        dynamicPayloadEnabled = true;
    } else {
        radio_->disableDynamicPayloads();
        radio_->setPayloadSize(cfg.fixedPayloadSize);
        dynamicPayloadEnabled = false;
    }

    // Pipes
    radio_->stopListening();
    radio_->flush_rx();

    radio_->openReadingPipe(cfg.pipe, cfg.addr);

    // radio_->setAutoAck(false);

    return true;
}

bool Rf24Service::initTx(const Rf24Config& cfg) {
    if (!isInitialized || radio_ == nullptr) return false;

    radio_->stopListening();
    radio_->stopConstCarrier();
    radio_->powerUp();
    delay(5);
    radio_->flush_tx();
    radio_->flush_rx();
    radio_->setChannel(cfg.channel);
    radio_->setAddressWidth(cfg.addrLen); 

    // CRC
    if (cfg.crcBits == 0) radio_->disableCRC();
    else if (cfg.crcBits == 8) radio_->setCRCLength(RF24_CRC_8);
    else radio_->setCRCLength(RF24_CRC_16);

    // Data rate
    if (cfg.dataRate == 0) radio_->setDataRate(RF24_250KBPS);
    else if (cfg.dataRate == 2) radio_->setDataRate(RF24_2MBPS);
    else radio_->setDataRate(RF24_1MBPS);

    // Payload
    if (cfg.dynamicPayloads) {
        radio_->enableDynamicPayloads();
        dynamicPayloadEnabled = true;
    } else {
        radio_->disableDynamicPayloads();
        radio_->setPayloadSize(cfg.fixedPayloadSize);
        dynamicPayloadEnabled = false;
    }

    radio_->stopListening();
    radio_->flush_tx();

    radio_->openWritingPipe(cfg.addr);

    return true;
}

void Rf24Service::setChannel(uint8_t channel) {
    if (isInitialized) {
        radio_->setChannel(channel);
    }
}

uint8_t Rf24Service::getChannel() {
    if (isInitialized) {
        return radio_->getChannel();
    }
    return 0;
}

void Rf24Service::powerUp() {
    if (isInitialized) {
        radio_->powerUp();
    }
}

void Rf24Service::powerDown(bool hard) {
    if (!isInitialized) {
        return;
    }

    if (hard) {
        radio_->powerDown();
        return;
    }
    
    radio_->stopConstCarrier();
}

void Rf24Service::setPowerLevel(rf24_pa_dbm_e level) {
    if (isInitialized) {
        radio_->setPALevel(level);
    }
}

void Rf24Service::setPowerMax() {
    if (isInitialized) {
        radio_->setPALevel(RF24_PA_MAX);
        radio_->startConstCarrier(RF24_PA_MAX, 45);
    }
}

void Rf24Service::startListening() {
    if (isInitialized) {
        radio_->startListening();
    }
}

void Rf24Service::stopListening() {
    if (isInitialized) {
        radio_->stopListening();
    }
}

bool Rf24Service::availablePipe(uint8_t* pipe) {
    if (!isInitialized) return false;
    return radio_->available(pipe);
}

int Rf24Service::getRxPayloadLen() {
    if (dynamicPayloadEnabled) return radio_->getDynamicPayloadSize();
    return radio_->getPayloadSize();
}

void Rf24Service::setDataRate(rf24_datarate_e rate) {
    if (isInitialized) {
        radio_->setDataRate(rate);
    }
}

void Rf24Service::setCrcLength(rf24_crclength_e length) {
    if (isInitialized) {
        radio_->setCRCLength(length);
    }
}

void Rf24Service::openWritingPipe(uint64_t address) {
    if (isInitialized) {
        radio_->openWritingPipe(address);
    }
}

void Rf24Service::openReadingPipe(uint8_t number, uint64_t address) {
    if (isInitialized) {
        radio_->openReadingPipe(number, address);
    }
}

bool Rf24Service::send(const void* buf, uint8_t len) {
    if (!isInitialized) return false;
    bool ok = radio_->write(buf, len);
    return ok;
}

bool Rf24Service::available() {
    if (!isInitialized) return false;
    return radio_->available();
}

bool Rf24Service::receive(void* buf, uint8_t len) {
    if (!isInitialized) return false;
    radio_->read(buf, len);
    return true;
}

bool Rf24Service::receive(uint8_t* out, size_t outMax, uint8_t& outLen) {
    outLen = 0;
    if (!isInitialized || !radio_ || !out || outMax == 0) return false;

    uint8_t len = 0;

    // Try DPL first if enabled
    if (dynamicPayloadEnabled) {
        len = radio_->getDynamicPayloadSize();
    }

    // Fallback if invalid
    if (len < 1 || len > 32) {
        len = radio_->getPayloadSize();      // fixed payload size from radio
    }

    if (len < 1 || len > 32) {
        radio_->flush_rx();
        return false;
    }

    if (len > outMax) len = (uint8_t)outMax;

    radio_->read(out, len);
    outLen = len;
    return true;
}

bool Rf24Service::isChipConnected() {
    if (!isInitialized) return false;
    return radio_->isChipConnected();
}

bool Rf24Service::testCarrier() {
    if (!isInitialized) return false;
    return radio_->testCarrier();
}

bool Rf24Service::testRpd() {
    if (!isInitialized) return false;
    return radio_->testRPD();
}

void Rf24Service::flushRx() {
    if (!isInitialized) return;
    radio_->flush_rx();
}

void Rf24Service::flushTx() {
    if (!isInitialized) return;
    radio_->flush_tx();
}

void Rf24Service::initTembedPlus() {
    // POWER
    const int BOARD_PWR_EN   = 15;

    // TFT
    const int BOARD_TFT_CS   = 41;

    // TF card
    const int BOARD_SD_CS    = 13;

    // LoRa / CC1101 front-end
    const int BOARD_LORA_CS  = 12;

    // Disable others spi devices
    pinMode(41, OUTPUT);
    digitalWrite(41, HIGH);
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);

    // Power cycle of external rail
    pinMode(BOARD_PWR_EN, OUTPUT);
    digitalWrite(BOARD_PWR_EN, LOW);
    delay(50);
    digitalWrite(BOARD_PWR_EN, HIGH);
    delay(50);
}
