#pragma once

#if defined(DEVICE_WAVESHARE_S3_GEEK)

#include "Interfaces/IInput.h"
#include <Arduino.h>
#define GEEK_BOOT_BUTTON_PIN 0

class WaveshareS3GeekInput : public IInput {
public:
    WaveshareS3GeekInput();

    char handler() override;
    char readChar() override;
    void waitPress(uint32_t timeoutMs) override;

    void tick();
    void checkShutdownRequest();
    void shutdownToDeepSleep();

private:
    uint8_t readButtons();
    uint8_t checkLongPress();
    char lastInput;
    char lastButton;
    const uint8_t BTN_UP = 1;
    const uint8_t BTN_LONG = 4;
    const uint8_t BTN_SHUT = 8;
    const uint8_t LONG_PRESS_MIN = 3;
    const uint8_t LONG_PRESS_MIN_SHTDWN = 120;
};

#endif
