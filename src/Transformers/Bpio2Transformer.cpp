#include "Transformers/Bpio2Transformer.h"

#include <cctype>
#include <cstring>

namespace {

uint16_t readU16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t readU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

int32_t readI32(const uint8_t* p) {
    return static_cast<int32_t>(readU32(p));
}

void writeU16(uint8_t* p, uint16_t value) {
    p[0] = static_cast<uint8_t>(value & 0xFF);
    p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void writeU32(uint8_t* p, uint32_t value) {
    p[0] = static_cast<uint8_t>(value & 0xFF);
    p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

class TableView {
public:
    bool open(const uint8_t* data, size_t length, size_t objectPosition) {
        data_ = data;
        length_ = length;
        object_ = objectPosition;

        if (!data_ || object_ + 4 > length_) {
            return false;
        }

        const int32_t backOffset = readI32(data_ + object_);
        if (backOffset <= 0 || static_cast<size_t>(backOffset) > object_) {
            return false;
        }

        vtable_ = object_ - static_cast<size_t>(backOffset);
        if (vtable_ + 4 > length_) {
            return false;
        }

        vtableSize_ = readU16(data_ + vtable_);
        objectSize_ = readU16(data_ + vtable_ + 2);
        if (vtableSize_ < 4 || vtable_ + vtableSize_ > length_ ||
            objectSize_ < 4 || object_ + objectSize_ > length_) {
            return false;
        }
        return true;
    }

    bool has(size_t slot, size_t width = 1) const {
        const size_t entry = vtable_ + 4 + slot * 2;
        if (entry + 2 > vtable_ + vtableSize_) {
            return false;
        }
        const uint16_t offset = readU16(data_ + entry);
        return offset != 0 && offset + width <= objectSize_ && object_ + offset + width <= length_;
    }

    uint8_t getU8(size_t slot, uint8_t defaultValue = 0) const {
        const size_t pos = fieldPosition(slot, 1);
        return pos == INVALID ? defaultValue : data_[pos];
    }

    uint16_t getU16(size_t slot, uint16_t defaultValue = 0) const {
        const size_t pos = fieldPosition(slot, 2);
        return pos == INVALID ? defaultValue : readU16(data_ + pos);
    }

    uint32_t getU32(size_t slot, uint32_t defaultValue = 0) const {
        const size_t pos = fieldPosition(slot, 4);
        return pos == INVALID ? defaultValue : readU32(data_ + pos);
    }

    bool getBool(size_t slot, bool defaultValue = false) const {
        return getU8(slot, defaultValue ? 1 : 0) != 0;
    }

    bool getTable(size_t slot, TableView& table) const {
        size_t target = 0;
        if (!getOffsetTarget(slot, target)) {
            return false;
        }
        return table.open(data_, length_, target);
    }

    bool getString(size_t slot, Bpio2::StringView& result) const {
        result = {};
        size_t target = 0;
        if (!getOffsetTarget(slot, target) || target + 4 > length_) {
            return false;
        }
        const uint32_t stringLength = readU32(data_ + target);
        const size_t dataStart = target + 4;
        if (stringLength > length_ - dataStart || dataStart + stringLength >= length_) {
            return false;
        }
        if (data_[dataStart + stringLength] != 0) {
            return false;
        }
        result.data = reinterpret_cast<const char*>(data_ + dataStart);
        result.length = stringLength;
        return true;
    }

    bool getByteVector(size_t slot, const uint8_t*& bytes, size_t& count) const {
        bytes = nullptr;
        count = 0;
        size_t target = 0;
        if (!getOffsetTarget(slot, target) || target + 4 > length_) {
            return false;
        }
        const uint32_t vectorLength = readU32(data_ + target);
        const size_t dataStart = target + 4;
        if (vectorLength > length_ - dataStart) {
            return false;
        }
        bytes = data_ + dataStart;
        count = vectorLength;
        return true;
    }

    bool validateU32Vector(size_t slot) const {
        size_t target = 0;
        if (!getOffsetTarget(slot, target) || target + 4 > length_) {
            return false;
        }
        const uint32_t vectorLength = readU32(data_ + target);
        const size_t dataStart = target + 4;
        return vectorLength <= (length_ - dataStart) / sizeof(uint32_t);
    }

private:
    static constexpr size_t INVALID = static_cast<size_t>(-1);

    size_t fieldPosition(size_t slot, size_t width) const {
        if (!has(slot, width)) {
            return INVALID;
        }
        const size_t entry = vtable_ + 4 + slot * 2;
        return object_ + readU16(data_ + entry);
    }

    bool getOffsetTarget(size_t slot, size_t& target) const {
        const size_t field = fieldPosition(slot, 4);
        if (field == INVALID) {
            return false;
        }
        const uint32_t relative = readU32(data_ + field);
        if (relative == 0 || relative > length_ - field) {
            return false;
        }
        target = field + relative;
        return target < length_;
    }

    const uint8_t* data_ = nullptr;
    size_t length_ = 0;
    size_t object_ = 0;
    size_t vtable_ = 0;
    size_t vtableSize_ = 0;
    size_t objectSize_ = 0;
};

struct TableHandle {
    size_t object = 0;
    size_t fieldCount = 0;
};

class FlatBufferWriter {
public:
    FlatBufferWriter(uint8_t* output, size_t capacity)
        : output_(output), capacity_(capacity) {
        if (output_ && capacity_ >= 4) {
            std::memset(output_, 0, capacity_);
            position_ = 4;
            valid_ = true;
        }
    }

    bool valid() const { return valid_; }
    size_t size() const { return valid_ ? position_ : 0; }

    TableHandle createRootTable(size_t fieldCount) {
        TableHandle table = createTable(fieldCount);
        if (valid_) {
            writeU32(output_, static_cast<uint32_t>(table.object));
        }
        return table;
    }

    TableHandle createTable(size_t fieldCount) {
        TableHandle result;
        if (!valid_ || fieldCount > 64) {
            fail();
            return result;
        }

        align(2);
        const size_t vtable = position_;
        const size_t vtableSize = 4 + fieldCount * 2;
        const size_t objectSize = 4 + fieldCount * 4;
        if (!reserve(vtableSize)) {
            return result;
        }
        writeU16(output_ + vtable, static_cast<uint16_t>(vtableSize));
        writeU16(output_ + vtable + 2, static_cast<uint16_t>(objectSize));

        align(4);
        const size_t object = position_;
        if (!reserve(objectSize)) {
            return result;
        }
        writeU32(output_ + object, static_cast<uint32_t>(object - vtable));

        result.object = object;
        result.fieldCount = fieldCount;
        return result;
    }

    void setU8(const TableHandle& table, size_t slot, uint8_t value, bool present = true) {
        if (!present) return;
        const size_t field = markField(table, slot);
        if (field != INVALID) output_[field] = value;
    }

    void setBool(const TableHandle& table, size_t slot, bool value, bool present = true) {
        setU8(table, slot, value ? 1 : 0, present);
    }

    void setU16(const TableHandle& table, size_t slot, uint16_t value, bool present = true) {
        if (!present) return;
        const size_t field = markField(table, slot);
        if (field != INVALID) writeU16(output_ + field, value);
    }

    void setU32(const TableHandle& table, size_t slot, uint32_t value, bool present = true) {
        if (!present) return;
        const size_t field = markField(table, slot);
        if (field != INVALID) writeU32(output_ + field, value);
    }

    void setFloat(const TableHandle& table, size_t slot, float value, bool present = true) {
        if (!present) return;
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        setU32(table, slot, bits, true);
    }

    void setOffset(const TableHandle& table, size_t slot, size_t target) {
        const size_t field = markField(table, slot);
        if (field == INVALID || target <= field || target - field > UINT32_MAX) {
            fail();
            return;
        }
        writeU32(output_ + field, static_cast<uint32_t>(target - field));
    }

    size_t createString(const char* value) {
        if (!value) value = "";
        const size_t length = std::strlen(value);
        align(4);
        const size_t target = position_;
        if (length > UINT32_MAX || !reserve(4 + length + 1)) {
            return 0;
        }
        writeU32(output_ + target, static_cast<uint32_t>(length));
        std::memcpy(output_ + target + 4, value, length);
        output_[target + 4 + length] = 0;
        return target;
    }

    size_t createByteVector(const uint8_t* data, size_t length) {
        align(4);
        const size_t target = position_;
        if (length > UINT32_MAX || !reserve(4 + length)) {
            return 0;
        }
        writeU32(output_ + target, static_cast<uint32_t>(length));
        if (length && data) {
            std::memcpy(output_ + target + 4, data, length);
        }
        return target;
    }

    size_t createU32Vector(const uint32_t* data, size_t length) {
        align(4);
        const size_t target = position_;
        if (length > UINT32_MAX || !reserve(4 + length * 4)) {
            return 0;
        }
        writeU32(output_ + target, static_cast<uint32_t>(length));
        for (size_t i = 0; i < length; ++i) {
            writeU32(output_ + target + 4 + i * 4, data ? data[i] : 0);
        }
        return target;
    }

    size_t createStringVector(const char* const* values, size_t count) {
        align(4);
        const size_t target = position_;
        if (count > UINT32_MAX || !reserve(4 + count * 4)) {
            return 0;
        }
        writeU32(output_ + target, static_cast<uint32_t>(count));
        for (size_t i = 0; i < count; ++i) {
            const size_t element = target + 4 + i * 4;
            const size_t stringTarget = createString(values ? values[i] : "");
            if (!valid_ || stringTarget <= element || stringTarget - element > UINT32_MAX) {
                fail();
                return 0;
            }
            writeU32(output_ + element, static_cast<uint32_t>(stringTarget - element));
        }
        return target;
    }

private:
    static constexpr size_t INVALID = static_cast<size_t>(-1);

    void fail() {
        valid_ = false;
        position_ = 0;
    }

    bool reserve(size_t count) {
        if (!valid_ || count > capacity_ - position_) {
            fail();
            return false;
        }
        std::memset(output_ + position_, 0, count);
        position_ += count;
        return true;
    }

    void align(size_t alignment) {
        if (!valid_) return;
        const size_t padding = (alignment - (position_ % alignment)) % alignment;
        reserve(padding);
    }

    size_t markField(const TableHandle& table, size_t slot) {
        if (!valid_ || slot >= table.fieldCount) {
            fail();
            return INVALID;
        }
        const size_t fieldOffset = 4 + slot * 4;
        const size_t vtable = table.object - readU32(output_ + table.object);
        const size_t entry = vtable + 4 + slot * 2;
        if (fieldOffset > UINT16_MAX || entry + 2 > capacity_) {
            fail();
            return INVALID;
        }
        writeU16(output_ + entry, static_cast<uint16_t>(fieldOffset));
        return table.object + fieldOffset;
    }

    uint8_t* output_ = nullptr;
    size_t capacity_ = 0;
    size_t position_ = 0;
    bool valid_ = false;
};

bool openRoot(const uint8_t* buffer, size_t length, TableView& root, const char*& error) {
    if (!buffer || length < 8) {
        error = "Flatbuffer packet too short";
        return false;
    }
    const uint32_t rootOffset = readU32(buffer);
    if (rootOffset < 4 || rootOffset >= length || !root.open(buffer, length, rootOffset)) {
        error = "Invalid flatbuffer root table";
        return false;
    }
    return true;
}

void parseModeConfiguration(const TableView& table, Bpio2::ModeConfiguration& config) {
    config.speed = table.getU32(0, 20000);
    config.dataBits = table.getU8(1, 8);
    config.parity = table.getBool(2, false);
    config.stopBits = table.getU8(3, 1);
    config.flowControl = table.getBool(4, false);
    config.signalInversion = table.getBool(5, false);
    config.clockStretch = table.getBool(6, false);
    config.clockPolarity = table.getBool(7, false);
    config.clockPhase = table.getBool(8, false);
    config.chipSelectIdle = table.getBool(9, true);
    config.submode = table.getU8(10, 0);
    config.txModulation = table.getU32(11, 0);
    config.rxSensor = table.getU8(12, 0);
}

size_t finishResponse(FlatBufferWriter& writer) {
    return writer.valid() && writer.size() <= Bpio2::MAX_PACKET_SIZE ? writer.size() : 0;
}

} // namespace

bool Bpio2Transformer::decodeRequest(const uint8_t* buffer, size_t length, Bpio2& request, const char*& error) {
    request = {};
    error = nullptr;

    if (length > Bpio2::MAX_PACKET_SIZE) {
        error = "Flatbuffer packet too large";
        return false;
    }

    TableView packet;
    if (!openRoot(buffer, length, packet, error)) {
        return false;
    }

    request.versionMajor = packet.getU8(0, 0);
    request.minimumVersionMinor = packet.getU16(1, 0);
    if (request.versionMajor != Bpio2::VERSION_MAJOR) {
        error = "Unsupported BPIO version, expected 2.x";
        return false;
    }
    if (request.minimumVersionMinor > Bpio2::VERSION_MINOR) {
        error = "BPIO minimum minor version not supported";
        return false;
    }

    const uint8_t rawType = packet.getU8(2, 0);
    if (rawType < static_cast<uint8_t>(Bpio2::RequestType::Status) ||
        rawType > static_cast<uint8_t>(Bpio2::RequestType::Data)) {
        error = "Unknown BPIO request type";
        return false;
    }
    request.type = static_cast<Bpio2::RequestType>(rawType);

    TableView contents;
    if (!packet.getTable(3, contents)) {
        error = "BPIO request contents missing";
        return false;
    }

    if (request.type == Bpio2::RequestType::Status) {
        // Status query filtering is optional; this implementation returns all supported fields,
        // but still validates the vector when the host supplied one.
        if (contents.has(0, 4)) {
            const uint8_t* query = nullptr;
            size_t queryLength = 0;
            if (!contents.getByteVector(0, query, queryLength)) {
                error = "Invalid status query vector";
                return false;
            }
        }
        return true;
    }

    if (request.type == Bpio2::RequestType::Configuration) {
        Bpio2::ConfigurationRequest& config = request.configuration;
        config.hasMode = contents.has(0, 4);
        if (config.hasMode && !contents.getString(0, config.mode)) {
            error = "Invalid mode string";
            return false;
        }

        TableView modeConfig;
        const bool modeConfigPresent = contents.has(1, 4);
        config.hasModeConfiguration = modeConfigPresent && contents.getTable(1, modeConfig);
        if (modeConfigPresent && !config.hasModeConfiguration) {
            error = "Invalid mode configuration";
            return false;
        }
        if (config.hasModeConfiguration) {
            parseModeConfiguration(modeConfig, config.modeConfiguration);
        }

        config.modeBitOrderMsb = contents.getBool(2, false);
        config.modeBitOrderLsb = contents.getBool(3, false);
        config.psuDisable = contents.getBool(4, false);
        config.psuEnable = contents.getBool(5, false);
        config.pullupDisable = contents.getBool(8, false);
        config.pullupEnable = contents.getBool(9, false);

        config.hasIoDirectionMask = contents.has(10, 1);
        config.ioDirectionMask = contents.getU8(10, 0);
        config.ioDirection = contents.getU8(11, 0);
        config.hasIoValueMask = contents.has(12, 1);
        config.ioValueMask = contents.getU8(12, 0);
        config.ioValue = contents.getU8(13, 0);

        config.hasLedResume = contents.has(14, 1);
        config.hasLedColor = contents.has(15, 4);
        if (config.hasLedColor && !contents.validateU32Vector(15)) {
            error = "Invalid LED color vector";
            return false;
        }
        config.hasPrintString = contents.has(16, 4);
        if (config.hasPrintString) {
            Bpio2::StringView printString;
            if (!contents.getString(16, printString)) {
                error = "Invalid print string";
                return false;
            }
        }
        config.hardwareBootloader = contents.getBool(17, false);
        config.hardwareReset = contents.getBool(18, false);
        config.hardwareSelftest = contents.getBool(19, false);
        return true;
    }

    Bpio2::DataRequest& data = request.data;
    data.startMain = contents.getBool(0, false);
    data.startAlt = contents.getBool(1, false);
    if (contents.has(2, 4) && !contents.getByteVector(2, data.dataWrite, data.dataWriteLength)) {
        error = "Invalid data_write vector";
        return false;
    }
    data.bytesRead = contents.getU16(3, 0);
    data.stopMain = contents.getBool(4, false);
    data.stopAlt = contents.getBool(5, false);

    if (data.dataWriteLength > Bpio2::MAX_WRITE_SIZE) {
        error = "Data write vector too long";
        return false;
    }
    const size_t expectedRead = data.startAlt ? data.dataWriteLength + data.bytesRead : data.bytesRead;
    if (expectedRead > Bpio2::MAX_READ_SIZE) {
        error = "Data read size too large";
        return false;
    }
    return true;
}

size_t Bpio2Transformer::buildErrorResponse(uint8_t* output, size_t capacity, const char* error) {
    FlatBufferWriter writer(output, capacity);
    const TableHandle packet = writer.createRootTable(3);
    const size_t errorString = writer.createString(error ? error : "Unknown error");
    writer.setOffset(packet, 0, errorString);
    return finishResponse(writer);
}

size_t Bpio2Transformer::buildConfigurationResponse(uint8_t* output, size_t capacity, const char* error) {
    FlatBufferWriter writer(output, capacity);
    const TableHandle packet = writer.createRootTable(3);
    writer.setU8(packet, 1, static_cast<uint8_t>(Bpio2::ResponseType::Configuration));

    const TableHandle response = writer.createTable(1);
    writer.setOffset(packet, 2, response.object);
    if (error && error[0]) {
        const size_t errorString = writer.createString(error);
        writer.setOffset(response, 0, errorString);
    }
    return finishResponse(writer);
}

size_t Bpio2Transformer::buildDataResponse(uint8_t* output,
                         size_t capacity,
                         const uint8_t* data,
                         size_t dataLength,
                         const char* error,
                         bool isAsync) {
    FlatBufferWriter writer(output, capacity);
    const TableHandle packet = writer.createRootTable(3);
    writer.setU8(packet, 1, static_cast<uint8_t>(Bpio2::ResponseType::Data));

    const TableHandle response = writer.createTable(3);
    writer.setOffset(packet, 2, response.object);
    if (error && error[0]) {
        const size_t errorString = writer.createString(error);
        writer.setOffset(response, 0, errorString);
    }
    if (data || dataLength == 0) {
        const size_t vector = writer.createByteVector(data, dataLength);
        writer.setOffset(response, 1, vector);
    }
    writer.setBool(response, 2, isAsync, isAsync);
    return finishResponse(writer);
}

size_t Bpio2Transformer::buildStatusResponse(uint8_t* output,
                           size_t capacity,
                           const Bpio2::StatusSnapshot& status,
                           const char* error) {
    FlatBufferWriter writer(output, capacity);
    const TableHandle packet = writer.createRootTable(3);
    writer.setU8(packet, 1, static_cast<uint8_t>(Bpio2::ResponseType::Status));

    const TableHandle response = writer.createTable(29);
    writer.setOffset(packet, 2, response.object);

    if (error && error[0]) {
        const size_t errorString = writer.createString(error);
        writer.setOffset(response, 0, errorString);
    }
    writer.setU8(response, 1, Bpio2::VERSION_MAJOR);
    writer.setU16(response, 2, Bpio2::VERSION_MINOR);
    writer.setU8(response, 3, status.hardwareMajor);
    writer.setU8(response, 4, status.hardwareMinor);
    writer.setU8(response, 5, status.firmwareMajor);
    writer.setU8(response, 6, status.firmwareMinor);

    const size_t gitHash = writer.createString(status.firmwareGitHash);
    writer.setOffset(response, 7, gitHash);
    const size_t firmwareDate = writer.createString(status.firmwareDate);
    writer.setOffset(response, 8, firmwareDate);
    const size_t modes = writer.createStringVector(status.modesAvailable, status.modesAvailableCount);
    writer.setOffset(response, 9, modes);
    const size_t currentMode = writer.createString(status.modeCurrent);
    writer.setOffset(response, 10, currentMode);
    const size_t pinLabels = writer.createStringVector(status.pinLabels, status.pinLabelCount);
    writer.setOffset(response, 11, pinLabels);

    writer.setBool(response, 12, status.bitOrderMsb, true);
    writer.setU32(response, 13, status.maxPacketSize);
    writer.setU32(response, 14, status.maxWrite);
    writer.setU32(response, 15, status.maxRead);

    // Unsupported Bus Pirate-specific hardware is reported as disabled/zero.
    writer.setBool(response, 16, false, true);
    writer.setU32(response, 17, 0);
    writer.setU32(response, 18, 0);
    writer.setU32(response, 19, 0);
    writer.setU32(response, 20, 0);
    writer.setBool(response, 21, false, true);
    writer.setBool(response, 22, false, true);
    const size_t adc = writer.createU32Vector(nullptr, 0);
    writer.setOffset(response, 23, adc);
    writer.setU8(response, 24, status.ioDirection);
    writer.setU8(response, 25, status.ioValue);
    writer.setFloat(response, 26, 0.0f);
    writer.setFloat(response, 27, 0.0f);
    writer.setU8(response, 28, 0);

    return finishResponse(writer);
}

bool Bpio2Transformer::cobsDecode(const uint8_t* input,
                size_t inputLength,
                uint8_t* output,
                size_t outputCapacity,
                size_t& outputLength) {
    outputLength = 0;
    if ((!input && inputLength) || !output) return false;

    size_t readIndex = 0;
    while (readIndex < inputLength) {
        const uint8_t code = input[readIndex++];
        if (code == 0) return false;

        const size_t copyLength = static_cast<size_t>(code - 1);
        if (copyLength > inputLength - readIndex || copyLength > outputCapacity - outputLength) {
            return false;
        }
        if (copyLength) {
            std::memcpy(output + outputLength, input + readIndex, copyLength);
            readIndex += copyLength;
            outputLength += copyLength;
        }

        if (code != 0xFF && readIndex < inputLength) {
            if (outputLength >= outputCapacity) return false;
            output[outputLength++] = 0;
        }
    }
    return true;
}

bool Bpio2Transformer::cobsEncode(const uint8_t* input,
                size_t inputLength,
                uint8_t* output,
                size_t outputCapacity,
                size_t& outputLength) {
    outputLength = 0;
    if ((!input && inputLength) || !output || outputCapacity == 0) return false;

    size_t readIndex = 0;
    size_t codeIndex = 0;
    uint8_t code = 1;
    outputLength = 1;

    while (readIndex < inputLength) {
        if (input[readIndex] == 0) {
            output[codeIndex] = code;
            codeIndex = outputLength;
            if (outputLength >= outputCapacity) return false;
            outputLength++;
            code = 1;
            readIndex++;
            continue;
        }

        if (outputLength >= outputCapacity) return false;
        output[outputLength++] = input[readIndex++];
        code++;

        if (code == 0xFF) {
            output[codeIndex] = code;
            codeIndex = outputLength;
            if (outputLength >= outputCapacity) return false;
            outputLength++;
            code = 1;
        }
    }

    output[codeIndex] = code;
    return true;
}
