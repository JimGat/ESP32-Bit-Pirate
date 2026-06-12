#if defined(DEVICE_WAVESHARE_S3_GEEK)

#include "WaveshareS3GeekInput.h"
#include "Inputs/InputKeys.h"
#include <esp_sleep.h>
#include <Arduino.h>

WaveshareS3GeekInput::WaveshareS3GeekInput()
    : lastInput(KEY_NONE),
      lastButton(0)
{
    pinMode(GEEK_BOOT_BUTTON_PIN, INPUT_PULLUP);
}

uint8_t WaveshareS3GeekInput::checkLongPress(){
    uint8_t iter = 0;
    while (digitalRead(GEEK_BOOT_BUTTON_PIN) == LOW && iter < LONG_PRESS_MIN_SHTDWN ){
        delay(100);
        iter++;
    }
    return iter;
}

uint8_t WaveshareS3GeekInput::readButtons(){
    uint8_t up = digitalRead(GEEK_BOOT_BUTTON_PIN) == LOW ? BTN_UP : 0;
    uint8_t longPress = 0;

    uint8_t longTime = checkLongPress();

    if ((longTime > LONG_PRESS_MIN) && longTime < LONG_PRESS_MIN_SHTDWN){
        longPress = BTN_LONG;
    } else if (longTime >= LONG_PRESS_MIN_SHTDWN){
        longPress = BTN_SHUT;
    }
    return up + longPress;
}

void WaveshareS3GeekInput::tick() {

    int buttons = readButtons();
    if ( buttons != lastButton){
        if (buttons == BTN_UP) {
            lastInput = KEY_ARROW_LEFT;
        } else if (buttons == (BTN_UP + BTN_LONG)) {
            lastInput = KEY_OK;
        } else if (buttons & BTN_SHUT){
            shutdownToDeepSleep();
        }
        lastButton = buttons;
    }
}

char WaveshareS3GeekInput::readChar() {
    tick();
    char c = lastInput;
    lastInput = KEY_NONE;
    return c;
}

char WaveshareS3GeekInput::handler() {
    while (true) {
        char c = readChar();
        if (c != KEY_NONE) return c;
        delay(5);
    }
}

void WaveshareS3GeekInput::waitPress(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (true) {
        if (readChar() != KEY_NONE) return;
        if (timeoutMs > 0 && (millis() - start) >= timeoutMs) return;
        delay(5);
    }
}

void WaveshareS3GeekInput::shutdownToDeepSleep() {
    delay(3000);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)GEEK_BOOT_BUTTON_PIN, 0);
    esp_deep_sleep_start();
}

#endif
