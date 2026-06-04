#include "UartService.h"
#include <algorithm>
#include <cmath>
#include <cstring>

void UartService::configure(unsigned long baud,
                            uint32_t config,
                            uint8_t rx,
                            uint8_t tx,
                            bool inverted,
                            HardwareSerial* serial,
                            bool noAllocation) {

    _serial = serial;

    _serial->end();

    gpio_reset_pin((gpio_num_t)rx);
    gpio_reset_pin((gpio_num_t)tx);

    _serial->begin(baud, config, rx, tx, inverted);
    installed = true;

    if (noAllocation) {
        return;
    }

    if (!buffersAllocated) {

        edgeIntervals = (uint32_t*) heap_caps_malloc(
            sizeof(uint32_t) * kMaxEdgeIntervals,
            MALLOC_CAP_INTERNAL
        );

        edgeCounts = (uint32_t*) heap_caps_malloc(
            sizeof(uint32_t) * 64,
            MALLOC_CAP_INTERNAL
        );

        if (!edgeIntervals || !edgeCounts) {
            if (edgeIntervals) heap_caps_free((void*)edgeIntervals);
            if (edgeCounts) heap_caps_free((void*)edgeCounts);
            edgeIntervals = nullptr;
            edgeCounts = nullptr;
            buffersAllocated = false;
            return;
        }

        memset((void*)edgeCounts, 0, sizeof(uint32_t) * 64);

        buffersAllocated = true;
    }

}

void UartService::release() {
    _serial->end();

    if (buffersAllocated) {
        heap_caps_free((void*)edgeIntervals);
        heap_caps_free((void*)edgeCounts);
        edgeIntervals = nullptr;
        edgeCounts = nullptr;
        buffersAllocated = false;
    }
}

void UartService::end() {
    if (!installed) return;
    _serial->end();
    installed = false;
}

bool UartService::isInstalled() const {
    return installed;
}

std::string UartService::readLine() {
    std::string input;
    bool lastWasCR = false;

    while (true) {
        if (!_serial->available()) continue;
        
        char c = _serial->read();

        if (c == '\r') {
            lastWasCR = true;
            _serial->println();
            break;
        }

        if (c == '\n') {
            if (!lastWasCR) {
                _serial->println();
                break;
            }
            continue;
        }

        if (c == '\b' || c == 127) {
            if (!input.empty()) {
                input.pop_back();
                _serial->print("\b \b");
            }
        } else {
            input += c;
            _serial->print(c);
            lastWasCR = false;
        }
    }

    return input;
}

void UartService::print(const std::string& msg) {
    _serial->print(msg.c_str());
}

void UartService::println(const std::string& msg) {
    _serial->println(msg.c_str());
}

bool UartService::available() const {
    return _serial->available();
}

char UartService::read() {
    return _serial->read();
}

void UartService::write(char c) {
    _serial->write(c);
}

void UartService::write(const char* str) {
    _serial->write(reinterpret_cast<const uint8_t*>(str), strlen(str));
}

void UartService::setRxFIFOFull(uint8_t fifoBytes){
    _serial->setRxFIFOFull(fifoBytes);
}

// Not sure this method is necessary as begin() sets the default values.
void UartService::setDefaultRxFIFOFull(){
    _serial->setRxFIFOFull(120);
}

void UartService::write(const std::string& str) {
    _serial->write(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
}

std::string UartService::executeByteCode(const std::vector<ByteCode>& bytecodes) {
    std::string result;
    uint32_t timeout = 2000; // 2 secondes
    uint32_t start;
    uint32_t received = 0;

    for (const ByteCode& code : bytecodes) {
        switch (code.getCommand()) {
            case ByteCodeEnum::Write:
                for (uint32_t i = 0; i < code.getRepeat(); ++i) {
                    _serial->write(code.getData());
                }
                break;

            case ByteCodeEnum::Read:
                start = millis();
                while (received < code.getRepeat() && (millis() - start < timeout)) {
                    if (_serial->available()) {
                        char c = _serial->read();
                        result += c;
                        ++received;
                    } else {
                        delay(10);
                    }
                }
                break;

            case ByteCodeEnum::DelayMs:
                delay(code.getRepeat());
                break;

            case ByteCodeEnum::DelayUs:
                delayMicroseconds(code.getRepeat());
                break;

            default:
                break;
        }
    }

    return result;
}

void UartService::switchBaudrate(unsigned long newBaud) {
    _serial->updateBaudRate(newBaud);
}

void UartService::flush() {
    _serial->flush();
}

void UartService::clearUartBuffer() {
    const size_t maxBytes = 512; // uart buffer size ?
    size_t count = 0;
    while (count < maxBytes && available()) {
        read();
        count++;
    }
}


uint32_t UartService::buildUartConfig(uint8_t dataBits, char parity, uint8_t stopBits) {
    uint32_t config = SERIAL_8N1;
    if (dataBits == 5) config = (stopBits == 2) ? SERIAL_5N2 : SERIAL_5N1;
    else if (dataBits == 6) config = (stopBits == 2) ? SERIAL_6N2 : SERIAL_6N1;
    else if (dataBits == 7) config = (stopBits == 2) ? SERIAL_7N2 : SERIAL_7N1;
    else if (dataBits == 8) config = (stopBits == 2) ? SERIAL_8N2 : SERIAL_8N1;

    if (parity == 'E') config |= 0x02;
    else if (parity == 'O') config |= 0x01;

    return config;
}

/*
XMODEM
*/
File* UartService::currentFile = nullptr;

void UartService::setXmodemBlockSize(int32_t size) {
    xmodemBlockSize = size;
}

void UartService::setXmodemIdSize(int8_t size) {
    xmodemIdSize = size;
}

int32_t UartService::getXmodemBlockSize() const {
    return xmodemBlockSize;
}

int8_t UartService::getXmodemIdSize() const {
    return xmodemIdSize;
}

void UartService::setXmodemCrc(bool enabled) {
    xmodemProtocol = enabled ? XModem::ProtocolType::CRC_XMODEM : XModem::ProtocolType::XMODEM;
}

void UartService::setXmodemReceiveHandler(bool (*handler)(void*, size_t, byte*, size_t)) {
    xmodem.setRecieveBlockHandler(handler);
}

void UartService::setXmodemSendHandler(void (*handler)(void*, size_t, byte*, size_t)) {
    xmodem.setBlockLookupHandler(handler);
}

void UartService::initXmodem() {
    xmodem.begin(*_serial, xmodemProtocol);
    xmodem.setDataSize(xmodemBlockSize);
    xmodem.setIdSize(xmodemIdSize);
}

void UartService::blockLookupHandler(void* blk_id, size_t idSize, byte* data, size_t dataSize) {
    if (!currentFile) {
        return;
    }

    uint32_t blockId = 0;
    for (size_t i = 0; i < idSize; ++i) {
        blockId = (blockId << 8) | ((uint8_t*)blk_id)[i];
    }

    size_t offset = blockId * dataSize;
    if (!currentFile->seek(offset)) {
        return;
    }

    // Read
    size_t readBytes = currentFile->read(data, dataSize);
    if (readBytes < dataSize) {
        memset(data + readBytes, 0x1A, dataSize - readBytes);
    }

    // Progression
    Serial.printf("Sending bloc: %u\n\r", (unsigned int)blockId);
}

bool UartService::receiveBlockHandler(void* blk_id, size_t idSize, byte* data, size_t dataSize) {
    if (!currentFile) {
        return false;
    }

    // Progression
    uint32_t blockId = 0;
    for (size_t i = 0; i < idSize; ++i) {
        blockId = (blockId << 8) | ((uint8_t*)blk_id)[i];
    }
    Serial.printf("Receiving bloc: %u\r\n", (unsigned int)blockId);

    // Write
    return currentFile->write(data, dataSize) == dataSize;
}

bool UartService::xmodemSendFile(File& file) {
    if (!file || file.isDirectory()) return false;

    // Xmodem Init
    initXmodem();
    currentFile = &file;
    xmodem.setBlockLookupHandler(blockLookupHandler);

    // Calculate
    size_t fileSize = file.size();
    size_t blockSize = xmodemBlockSize;
    size_t totalBlocks = (fileSize + blockSize - 1) / blockSize;
    const size_t idSize = xmodemIdSize;

    // Construct Ids
    byte* all_ids = (byte*)malloc(totalBlocks * idSize);
    for (size_t i = 0; i < totalBlocks; ++i) {
        unsigned long long blk_id = i + 1;
        for (size_t j = 0; j < idSize; ++j) {
            all_ids[i * idSize + j] = (blk_id >> (8 * (idSize - j - 1))) & 0xFF;
        }
    }

    // Data, lookup_handler will deal with empty data
    byte** dummy_data = (byte**)malloc(sizeof(byte*) * totalBlocks);
    size_t* dummy_lens = (size_t*)malloc(sizeof(size_t) * totalBlocks);
    for (size_t i = 0; i < totalBlocks; ++i) {
        dummy_data[i] = nullptr;
        dummy_lens[i] = blockSize;
    }

    // Configure container for send_bulk_data
    struct XModem::bulk_data container = {
        .data_arr = dummy_data,
        .len_arr = dummy_lens,
        .id_arr = all_ids,
        .count = totalBlocks
    };

    // Send
    bool result = xmodem.send_bulk_data(container);

    // Release
    free(all_ids);
    free(dummy_data);
    free(dummy_lens);
    currentFile = nullptr;

    return result;
}


bool UartService::xmodemReceiveToFile(File& file) {
    if (!file || file.isDirectory()) return false;

    // Init
    initXmodem();
    currentFile = &file;

    // Receive
    xmodem.setRecieveBlockHandler(receiveBlockHandler);
    bool ok = xmodem.receive();

    currentFile = nullptr;
    return ok;
}

void IRAM_ATTR UartService::onGpioEdge(void* arg) {
    uint32_t pin = (uint32_t)arg;

    uint32_t now = micros();
    if (lastEdgeTimeUs != 0 && intervalCount < kMaxEdgeIntervals) {
        edgeIntervals[intervalCount++] = now - lastEdgeTimeUs;
    }
    lastEdgeTimeUs = now;

    if (pin < 50) {
        edgeCounts[pin]++;
    }
}

UartService::PinActivity UartService::measureUartActivity(uint8_t pin, uint32_t windowMs, bool pullup) {
    edgeCounts[pin] = 0;
    lastEdgeTimeUs = 0;
    intervalCount = 0;
    
    // already installed by the kb for the adv
    #ifndef DEVICE_CARDPUTERADV
        if (!isrInstalled) {
            gpio_install_isr_service(0);
            isrInstalled = true;
        }
    #endif

    // Configure GPIO
    gpio_config_t io = {};
    io.intr_type = GPIO_INTR_ANYEDGE;
    io.mode = GPIO_MODE_INPUT;
    io.pin_bit_mask = (1ULL << pin);
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.pull_up_en = pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    gpio_config(&io);

    // Attach ISR and measure
    gpio_isr_handler_add((gpio_num_t)pin, &UartService::onGpioEdge, (void*)pin);
    uint32_t start = millis();
    while ((millis() - start) < windowMs) {
        delay(1);
    }
    gpio_isr_handler_remove((gpio_num_t)pin);

    // Calculate edges per second
    uint32_t edges = edgeCounts[pin];
    float eps = (windowMs > 0) ? (edges * 1000.0f / windowMs) : 0.0f;

    // Baud estimating
    uint32_t approxBaud = 0;
    if (intervalCount > 8) {
        std::vector<uint32_t> tmp(edgeIntervals, edgeIntervals + intervalCount);
        std::sort(tmp.begin(), tmp.end());

        // Remove tiny ISR/glitch intervals that can corrupt baud estimation.
        tmp.erase(
            std::remove_if(tmp.begin(), tmp.end(), [](uint32_t us) { return us < 2 || us > 200000; }),
            tmp.end()
        );

        if (tmp.size() > 8) {
            float bestScore = 0.0f;
            float bestError = 1000000.0f;
            uint32_t bestBaud = 0;
            size_t bestMatches = 0;
            const size_t minMatches = std::max<size_t>(6, tmp.size() / 2);
            const float minScore = static_cast<float>(minMatches) * 0.20f;

            // Score each standard baud by how well measured intervals fit integer bit-time multiples.
            for (size_t i = 0; i < kBaudRatesCount; i++) {
                uint32_t candidate = kBaudRates[i];
                if (candidate < 1200 || candidate > 400000) {
                    continue;
                }

                float bitUs = 1000000.0f / static_cast<float>(candidate);
                float relativeTolUs = bitUs * 0.20f;
                float tolUs = (relativeTolUs > 1.0f) ? relativeTolUs : 1.0f;
                float score = 0.0f;
                float error = 0.0f;
                size_t matches = 0;

                for (uint32_t intervalUs : tmp) {
                    float ratio = static_cast<float>(intervalUs) / bitUs;
                    int nearest = static_cast<int>(std::round(ratio));
                    if (nearest < 1) {
                        nearest = 1;
                    }

                    float expectedUs = bitUs * static_cast<float>(nearest);
                    float errUs = std::fabs(static_cast<float>(intervalUs) - expectedUs);

                    if (errUs <= tolUs) {
                        // Favor direct/short multiples, they carry more baud information.
                        float weight = 1.0f / static_cast<float>(nearest);
                        score += weight;
                        error += (errUs / tolUs) * weight;
                        matches++;
                    }
                }

                if (matches < minMatches || score < minScore) {
                    continue;
                }

                float normalizedError = error / score;
                if (score > (bestScore + 0.05f) ||
                    (std::fabs(score - bestScore) <= 0.05f && normalizedError < bestError) ||
                    (std::fabs(score - bestScore) <= 0.05f && normalizedError == bestError && matches > bestMatches)) {
                    bestMatches = matches;
                    bestScore = score;
                    bestError = normalizedError;
                    bestBaud = candidate;
                }
            }

            if (bestBaud != 0) {
                approxBaud = bestBaud;
            } else {
                // Fallback: use lower percentile interval to avoid /2 median harmonics.
                size_t p30 = (tmp.size() * 3) / 10;
                uint32_t base = tmp[p30];
                if (base > 0) {
                    approxBaud = 1000000UL / base;
                    if (approxBaud < 1200 || approxBaud > 400000) {
                        approxBaud = 0;
                    } else {
                        approxBaud = (approxBaud / 100) * 100;
                    }
                }
            }
        }
    }

    return { pin, edges, eps, approxBaud };
}

std::vector<UartService::PinActivity> UartService::scanUartActivity(const std::vector<uint8_t>& pins,
                                                                    uint32_t windowMs,
                                                                    uint32_t minEdges,
                                                                    bool pullup) {
    std::vector<PinActivity> out;
    out.reserve(pins.size());

    for (auto pin : pins) {
        auto r = measureUartActivity(pin, windowMs, pullup);
        if (r.edges >= minEdges) {
            out.push_back(r);
        }
    }

    std::sort(out.begin(), out.end(), [](const PinActivity& a, const PinActivity& b) {
        return a.edges > b.edges;
    });

    return out;
}

uint32_t UartService::detectBaudByEdge(
    uint8_t pin,
    uint32_t totalMs,
    uint32_t windowMs,
    uint32_t minEdges,
    bool pullup
) {
    std::map<uint32_t, uint32_t> votes;
    uint32_t bestBaud = 0;
    uint32_t bestVotes = 0;

    uint32_t consecutiveBaud = 0;
    uint32_t consecutiveCount = 0;
    const uint32_t burstEdgeThreshold = std::max<uint32_t>(24, minEdges * 6);

    unsigned long start = millis();

    while (millis() - start < totalMs) {
        auto a = measureUartActivity(pin, windowMs, pullup);

        // not enough edges or no valid baud
        if (a.edges < minEdges || a.approxBaud == 0) {
            consecutiveBaud = 0;
            consecutiveCount = 0;
            continue;
        }

        // to closest standard baudrate
        uint32_t approx = a.approxBaud;
        uint32_t snapped = kBaudRates[0];
        uint32_t bestDiff = (approx > snapped) ? (approx - snapped) : (snapped - approx);

        for (size_t i = 1; i < kBaudRatesCount; i++) {
            uint32_t b = kBaudRates[i];
            uint32_t diff = (approx > b) ? (approx - b) : (b - approx);
            if (diff < bestDiff) {
                bestDiff = diff;
                snapped = b;
            }
        }

        if (a.edges >= burstEdgeThreshold && bestDiff == 0) {
            return snapped;
        }

        votes[snapped]++;

        // track best
        if (votes[snapped] > bestVotes) {
            bestVotes = votes[snapped];
            bestBaud = snapped;
        }

        // check for consecutive agreement
        if (snapped == consecutiveBaud) consecutiveCount++;
        else {
            consecutiveBaud = snapped;
            consecutiveCount = 1;
        }

        if (consecutiveCount >= 2) return consecutiveBaud;
        if (bestVotes >= 3) return bestBaud;
    }

    return 0;
}
