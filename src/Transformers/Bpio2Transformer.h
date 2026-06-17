#pragma once

#include <cstddef>
#include <cstdint>

#include "Models/Bpio2.h"

// Converts between the BPIO2 wire representation and the firmware models.
// It owns the COBS framing and the bounded FlatBuffers compatible codec.

class Bpio2Transformer {
public:
    static bool decodeRequest(const uint8_t* buffer,
                              size_t length,
                              Bpio2& request,
                              const char*& error);

    static size_t buildErrorResponse(uint8_t* output,
                                     size_t capacity,
                                     const char* error);

    static size_t buildConfigurationResponse(uint8_t* output,
                                             size_t capacity,
                                             const char* error = nullptr);

    static size_t buildDataResponse(uint8_t* output,
                                    size_t capacity,
                                    const uint8_t* data,
                                    size_t dataLength,
                                    const char* error = nullptr,
                                    bool isAsync = false);

    static size_t buildStatusResponse(uint8_t* output,
                                      size_t capacity,
                                      const Bpio2::StatusSnapshot& status,
                                      const char* error = nullptr);

    static bool cobsDecode(const uint8_t* input,
                           size_t inputLength,
                           uint8_t* output,
                           size_t outputCapacity,
                           size_t& outputLength);

    static bool cobsEncode(const uint8_t* input,
                           size_t inputLength,
                           uint8_t* output,
                           size_t outputCapacity,
                           size_t& outputLength);
};
