#include "TwoWireService.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

void TwoWireService::configure(uint8_t clk, uint8_t io, uint8_t rst) {
    clkPin = clk;
    ioPin = io;
    rstPin = rst;

    gpio_reset_pin((gpio_num_t)clkPin);
    gpio_reset_pin((gpio_num_t)rstPin);
    gpio_reset_pin((gpio_num_t)ioPin);

    gpio_set_direction((gpio_num_t)clkPin, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)rstPin, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)ioPin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode((gpio_num_t)ioPin, GPIO_PULLUP_ONLY);

    gpio_set_level((gpio_num_t)clkPin, 0);
    gpio_set_level((gpio_num_t)rstPin, 0);
    gpio_set_level((gpio_num_t)ioPin, 1); // release
}

void TwoWireService::end() {
    gpio_set_level((gpio_num_t)clkPin, 0);
    gpio_set_level((gpio_num_t)rstPin, 0);

    gpio_set_direction((gpio_num_t)clkPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)clkPin, GPIO_FLOATING);

    gpio_set_direction((gpio_num_t)rstPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)rstPin, GPIO_FLOATING);

    gpio_set_direction((gpio_num_t)ioPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)ioPin, GPIO_FLOATING);
}

void TwoWireService::setRST(bool level) {
    gpio_set_level((gpio_num_t)rstPin, level ? 1 : 0);
}

void TwoWireService::setCLK(bool level) {
    gpio_set_level((gpio_num_t)clkPin, level ? 1 : 0);
}

void TwoWireService::setIO(bool level) {
    gpio_set_level((gpio_num_t)ioPin, level ? 1 : 0);
}

bool TwoWireService::readIO() {
    // gpio_set_direction((gpio_num_t)ioPin, GPIO_MODE_INPUT);
    return gpio_get_level((gpio_num_t)ioPin);
}

void TwoWireService::pulseClock() {
    setCLK(false);
    delayMicroseconds(5);
    setCLK(true);
    delayMicroseconds(5);
    setCLK(false);
}

void TwoWireService::writeBit(bool bit) {
    setIO(bit);
    pulseClock();
}

bool TwoWireService::readBit() {
    setCLK(true);
    delayMicroseconds(5);
    bool bit = readIO();
    setCLK(false);
    delayMicroseconds(5);
    return bit;
}

void TwoWireService::writeByte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        writeBit((byte >> i) & 1);
    }
}

uint8_t TwoWireService::readByte() {
    uint8_t b = 0;
    for (uint8_t i = 0; i < 8; i++) {
        bool bit = readBit();
        if (bit) b |= (1 << i);
    }
    return b;
}

void TwoWireService::sendStart() {
    setIO(true);
    setCLK(true);
    delayMicroseconds(5);
    setIO(false);
    delayMicroseconds(5);
    setCLK(false);
}

void TwoWireService::sendStop() {
    setIO(false);
    setCLK(true);
    delayMicroseconds(5);
    setIO(true);
    delayMicroseconds(5);
    setCLK(false);
}

void TwoWireService::sendCommand(uint8_t a, uint8_t b, uint8_t c) {
    sendStart();
    writeByte(a);
    writeByte(b);
    writeByte(c);
    sendStop();
}

std::vector<uint8_t> TwoWireService::readResponse(uint16_t len) {
    std::vector<uint8_t> data;
    for (uint16_t i = 0; i < len; i++) {
        data.push_back(readByte());
    }
    return data;
}

void TwoWireService::sendClocks(uint16_t ticks) {
    for (uint16_t i = 0; i < ticks; i++) {
        pulseClock();
    }
}

bool TwoWireService::waitIOHigh(uint32_t maxTicks) {
    for (uint32_t i = 0; i < maxTicks; ++i) {
        pulseClock();
        if (readIO()) return true;
    }
    return false; // timeout
}

std::vector<uint8_t> TwoWireService::performSmartCardAtr() {
    // dummy tick to 'load' the clock, avoiding us delay on first call
    setCLK(true);
    delayMicroseconds(5);
    setCLK(false);

    // Start ATR
    setIO(false);     // IO to output
    setRST(false);    // RST LOW
    delay(1);         // wait
    setRST(true);     // RST release
    pulseClock();     // Clock tick
    delayMicroseconds(50); // wait
    setRST(false);    // RST LOW
    setIO(true);      // IO to input

    return readResponse(4);
}

std::string TwoWireService::parseSmartCardAtr(const std::vector<uint8_t>& atr) {
    char line[64];
    std::string out;

    if (atr.size() < 4) {
        snprintf(line, sizeof(line), "ATR too short (%d bytes)\r\n", (int)atr.size());
        return line;
    }

    const sle44xx_atr_t* head = reinterpret_cast<const sle44xx_atr_t*>(atr.data());

    snprintf(line, sizeof(line), "   ATR: 0x%02X 0x%02X 0x%02X 0x%02X\r\n", atr[0], atr[1], atr[2], atr[3]);
    out += line;

    snprintf(line, sizeof(line), "   Protocol Type: %s (%d)\r\n",
             (head->protocol_type == 0b1010) ? "S" : "unknown",
             head->protocol_type);
    out += line;

    out += parseSmartCardStructureIdentifier(head->structure_identifier);

    out += "   Read Mode: ";
    out += head->read_with_defined_length ? "Defined Length\r\n" : "Read to End\r\n";

    if (head->data_units == 0b0000) {
        out += "   Data Units: Undefined\r\n";
    } else {
        int size = 1 << (head->data_units + 6);
        snprintf(line, sizeof(line), "   Data Units: %d\r\n", size);
        out += line;
    }

    int bit_len = 1 << head->data_units_bits;
    snprintf(line, sizeof(line), "   Data Unit Bit Length: %d\r\n", bit_len);
    out += line;

    return out;
}

std::string TwoWireService::parseSmartCardStructureIdentifier(uint8_t id) {
    std::string out = "   Structure Identifier: ";
    switch (id) {
        case 0b000:
            out += "Reserved for ISO/IEC Use\r\n";
            break;
        case 0b010:
            out += "Standard Memory Structure (Type 1)\r\n";
            break;
        case 0b110:
            out += "Proprietary Memory\r\n";
            break;
        default:
            out += "Application-Specific\r\n";
            break;
    }
    return out;
}

uint8_t TwoWireService::parseSmartCardRemainingAttempts(uint8_t statusByte) {
    resetSmartCard();
    uint8_t attemptsBits = statusByte & 0x07;
    int attempts = 0;
    for (int i = 0; i < 3; ++i) {
        if (attemptsBits & (1 << i)) {
            attempts++;
        }
    }
    return attempts;
}

std::vector<uint8_t> TwoWireService::dumpSmartCardFullMemory() {
    resetSmartCard();
    std::vector<uint8_t> dump;

    // Main memory 256 bytes
    sendCommand(0x30, 0x00, 0x00);
    for (int i = 0; i < 256; ++i) {
        dump.push_back(readByte());
    }

    // Security memory 4 bytes
    sendCommand(0x31, 0x00, 0x00);
    for (int i = 0; i < 4; ++i) {
        dump.push_back(readByte());
    }

    // Protection memory 4 bytes
    sendCommand(0x34, 0x00, 0x00);
    for (int i = 0; i < 4; ++i) {
        dump.push_back(readByte());
    }

    return dump; // total: 264 bytes
}

void TwoWireService::resetSmartCard() {
    sendClocks(256);
}

void TwoWireService::updateSmartCardSecurityAttempts(uint8_t pattern) {
    sendCommand(0x39, 0x00, pattern);
    resetSmartCard();
}

void TwoWireService::compareSmartCardVerificationData(uint8_t address, uint8_t value) {
    sendCommand(0x33, address, value);
    resetSmartCard();
}

void TwoWireService::writeSmartCardSecurityMemory(uint8_t address, uint8_t value) {
    sendCommand(0x39, address, value);
    resetSmartCard();
}

void TwoWireService::writeSmartCardProtectionMemory(uint8_t address, uint8_t value) {
    sendCommand(0x3C, address, value);
    resetSmartCard();
}

bool TwoWireService::writeSmartCardMainMemory(uint8_t address, uint8_t value) {
    sendCommand(0x38, address, value);
    resetSmartCard();

    // Read to verify
    auto readBack = readSmartCardMainMemory(address, 1);
    return !readBack.empty() && readBack[0] == value;
}

std::vector<uint8_t> TwoWireService::readSmartCardMainMemory(uint8_t startAddress, uint16_t length) {
    std::vector<uint8_t> buf;
    sendCommand(0x30, startAddress, 0x00);
    for (uint16_t i = 0; i < length; ++i) {
        buf.push_back(readByte());
    }
    return buf;
}

std::vector<uint8_t> TwoWireService::readSmartCardSecurityMemory() {
    sendCommand(0x31, 0x00, 0x00);
    return readResponse(4);
}

std::vector<uint8_t> TwoWireService::readSmartCardProtectionMemory() {
    sendCommand(0x34, 0x00, 0x00);
    return readResponse(4);
}

bool TwoWireService::updateSmartCardPSC(const uint8_t psc[3]) {
    writeSmartCardSecurityMemory(1, psc[0]);
    writeSmartCardSecurityMemory(2, psc[1]);
    writeSmartCardSecurityMemory(3, psc[2]);

    std::vector<uint8_t> secmem = readSmartCardSecurityMemory();
    if (secmem.size() < 4) {
        return false;
    }

    if (secmem[1] != psc[0] || secmem[2] != psc[1] || secmem[3] != psc[2]) {
        return false;
    }
    return true;
}

bool TwoWireService::getSmartCardPSC(uint8_t out_psc[3]) {
    std::vector<uint8_t> secmem = readSmartCardSecurityMemory();
    if (secmem.size() < 4) {
        return false;
    }
    out_psc[0] = secmem[1];
    out_psc[1] = secmem[2];
    out_psc[2] = secmem[3];
    return true;
}

bool TwoWireService::protectSmartCard() {
    uint32_t value = 0xFFFFFFFF;

    for (uint8_t i = 0; i < 4; ++i) {
        uint8_t byte = (value >> (8 * i)) & 0xFF;
        writeSmartCardProtectionMemory(i, byte);
    }

    // Lire back pour vérification
    auto check = readSmartCardProtectionMemory();
    if (check.size() != 4) return false;

    uint32_t readValue = 
        (check[3] << 24) |
        (check[2] << 16) |
        (check[1] << 8)  |
        (check[0]);

    return (readValue & value) == value;
}

bool TwoWireService::unlockSmartCard(const uint8_t psc[3]) {
    // Read sec mem
    std::vector<uint8_t> secmem = readSmartCardSecurityMemory();
    if (secmem.size() < 1) {
        return false;
    }

    // Verify attempts
    uint8_t sec = secmem[0];
    uint8_t pattern = 0;

    if (sec & 0b100) {
        pattern = 0b011;
    } else if (sec & 0b010) {
        pattern = 0b101;
    } else if (sec & 0b001) {
        pattern = 0b110;
    } else {
        // No attempts left
        return false;
    }

    // Update security attempts
    updateSmartCardSecurityAttempts(pattern);

    // Send the PSC
    compareSmartCardVerificationData(1, psc[0]);
    compareSmartCardVerificationData(2, psc[1]);
    compareSmartCardVerificationData(3, psc[2]);

    // Reinitialize security memory
    writeSmartCardSecurityMemory(0, 0xFF);

    // Read back to verify
    std::vector<uint8_t> secmemAfter = readSmartCardSecurityMemory();
    if (secmemAfter.size() < 1 || secmemAfter[0] != 0x07) {
        return false;
    }

    return true;
}

// ================== SNIFFER: helpers ==================

bool TwoWireService::ensureSnifferBufferAllocated() {
    if (sn_q != nullptr) {
        return true;
    }

    sn_q = static_cast<volatile SniffEvent*>(
        heap_caps_malloc(SNIFF_Q_SIZE * sizeof(SniffEvent), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
    );
    if (sn_q == nullptr) {
        return false;
    }

    sn_qHead = 0;
    sn_qTail = 0;
    return true;
}

inline void IRAM_ATTR TwoWireService::pushEvent(uint8_t type, uint8_t data) {
    if (sn_q == nullptr) {
        return;
    }

    portENTER_CRITICAL_ISR(&sn_mux);

    uint16_t next = (sn_qHead + 1) % SNIFF_Q_SIZE;
    if (next != sn_qTail) {
        sn_q[sn_qHead].type = type;
        sn_q[sn_qHead].data = data;
        sn_qHead = next;
    }

    portEXIT_CRITICAL_ISR(&sn_mux);
}

bool TwoWireService::popEvent(uint8_t& type, uint8_t& data) {
    if (sn_q == nullptr) return false;
    if (sn_qTail == sn_qHead) return false;

    portENTER_CRITICAL(&sn_mux);
    if (sn_qTail == sn_qHead) { portEXIT_CRITICAL(&sn_mux); return false; }

    // Read fields from the volatile element
    type = sn_q[sn_qTail].type;
    data = sn_q[sn_qTail].data;

    sn_qTail = (sn_qTail + 1) % SNIFF_Q_SIZE;
    portEXIT_CRITICAL(&sn_mux);
    return true;
}

// ================== SNIFFER ==================

void IRAM_ATTR TwoWireService::clk_isr_thunk(void* arg) {
    reinterpret_cast<TwoWireService*>(arg)->onClkRisingISR();
}

void IRAM_ATTR TwoWireService::io_isr_thunk(void* arg) {
    reinterpret_cast<TwoWireService*>(arg)->onIoChangeISR();
}

void IRAM_ATTR TwoWireService::onClkRisingISR() {
    if (!sn_active) return;

    // Validate START condition
    if (sn_startPending) {
        sn_startPending = false;
        sn_inFrame = true;
        pushEvent(/*START=*/1, 0);
    }

    if (!sn_inFrame) return;  // Ignore bits outside of a frame

    int io = gpio_get_level((gpio_num_t)ioPin);
    if (io) sn_currentByte |= (1u << sn_bitIndex);
    sn_bitIndex++;
    if (sn_bitIndex >= 8) {
        pushEvent(/*DATA=*/3, sn_currentByte);
        sn_bitIndex = 0;
        sn_currentByte = 0;
    }
}

void IRAM_ATTR TwoWireService::onIoChangeISR() {
    if (!sn_active) return;

    int clk = gpio_get_level((gpio_num_t)clkPin);
    int io  = gpio_get_level((gpio_num_t)ioPin);

    if (clk) {
        // START we wait for the first CLK
        if (sn_lastIO == 1 && io == 0) {
            if (!sn_inFrame && !sn_startPending) {
                sn_bitIndex = 0;
                sn_currentByte = 0;
                sn_startPending = true;   // START pending
            }
        }
        // STOP
        else if (sn_lastIO == 0 && io == 1) {
            if (sn_inFrame) {
                sn_inFrame = false;
                sn_bitIndex = 0;
                sn_currentByte = 0;
                pushEvent(/*STOP=*/2, 0);
            } else if (sn_startPending) {
                // START cancelled (no bits arrived)
                sn_startPending = false;
            }
        }
    }
    sn_lastIO = (uint8_t)io;
}

bool TwoWireService::startSniffer() {
    if (clkPin == 0xFF || ioPin == 0xFF) return false;
    if (!ensureSnifferBufferAllocated()) return false;

    // Install ISR service
    static bool isr_service_installed = false;
    if (!isr_service_installed) {
        esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return false;
        }
        isr_service_installed = true;
    }

    // Set CLK/IO as input non intrusive
    gpio_set_direction((gpio_num_t)clkPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)clkPin, GPIO_FLOATING);

    gpio_set_direction((gpio_num_t)ioPin,  GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)ioPin,  GPIO_PULLUP_ONLY); 

    // Init
    sn_active = false;
    sn_inFrame      = false;
    sn_startPending = false;
    sn_bitIndex = 0;
    sn_currentByte = 0;
    sn_lastIO = (uint8_t)gpio_get_level((gpio_num_t)ioPin);
    sn_qHead = sn_qTail = 0;

    // Interrupt types
    gpio_set_intr_type((gpio_num_t)clkPin,
        SNIFF_SAMPLE_ON_NEGEDGE ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE);
    gpio_set_intr_type((gpio_num_t)ioPin,  GPIO_INTR_ANYEDGE);

    // Handlers
    gpio_isr_handler_add((gpio_num_t)clkPin, &TwoWireService::clk_isr_thunk, this);
    gpio_isr_handler_add((gpio_num_t)ioPin,  &TwoWireService::io_isr_thunk,  this);

    gpio_intr_enable((gpio_num_t)clkPin);
    gpio_intr_enable((gpio_num_t)ioPin);

    sn_active = true;
    return true;
}

void TwoWireService::stopSniffer() {
    if (!sn_active) return;
    sn_active = false;

    // Detach ISRs
    gpio_intr_disable((gpio_num_t)clkPin);
    gpio_intr_disable((gpio_num_t)ioPin);

    gpio_isr_handler_remove((gpio_num_t)clkPin);
    gpio_isr_handler_remove((gpio_num_t)ioPin);

    // ---- Restore "bus active" pin configuration ----
    // CLK push-pull output (idle low)
    gpio_set_direction((gpio_num_t)clkPin, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode((gpio_num_t)clkPin, GPIO_FLOATING);
    gpio_set_level((gpio_num_t)clkPin, 0);

    // RST push-pull output (idle low) - if you want it restored too
    if (rstPin != 0xFF) {
        gpio_set_direction((gpio_num_t)rstPin, GPIO_MODE_OUTPUT);
        gpio_set_pull_mode((gpio_num_t)rstPin, GPIO_FLOATING);
        gpio_set_level((gpio_num_t)rstPin, 0);
    }

    // IO open-drain with input enabled + pull-up, released
    gpio_set_direction((gpio_num_t)ioPin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode((gpio_num_t)ioPin, GPIO_PULLUP_ONLY);
    gpio_set_level((gpio_num_t)ioPin, 1); // released
}

void TwoWireService::releaseSniffer() {
    stopSniffer();

    portENTER_CRITICAL(&sn_mux);
    volatile SniffEvent* oldQueue = sn_q;
    sn_q = nullptr;
    sn_qHead = 0;
    sn_qTail = 0;
    sn_inFrame = false;
    sn_startPending = false;
    sn_bitIndex = 0;
    sn_currentByte = 0;
    portEXIT_CRITICAL(&sn_mux);

    heap_caps_free(const_cast<SniffEvent*>(oldQueue));
}

bool TwoWireService::getNextSniffEvent(uint8_t& type, uint8_t& data) {
    return popEvent(type, data);
}

void TwoWireService::printSniffOnce(Stream& out) {
    uint8_t t, d;
    while (getNextSniffEvent(t, d)) {
        if (t == 1)      { out.print('['); }
        else if (t == 2) { out.println(']'); }
        else if (t == 3) { char buf[6]; sprintf(buf, " 0x%02X", d); out.print(buf); }
        else             { out.println("U"); }
    }
}
