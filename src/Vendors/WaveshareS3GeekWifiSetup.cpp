#if defined(DEVICE_WAVESHARE_S3_GEEK)

#include "WaveshareS3GeekWifiSetup.h"

#include <WiFi.h>
#include <Preferences.h>
#include <esp_sleep.h>

#include "Inputs/InputKeys.h"
#include "Views/WaveshareS3GeekDeviceView.h"

static lgfx::LGFX_Device* g_tft = nullptr;

static char lastInput = KEY_NONE;
static bool lastButton = false;
// Some constant used in this module
const uint8_t BTN_UP = 1;
const uint8_t BTN_LONG = 4;
const uint8_t BTN_SHUT = 8;
const uint8_t LONG_PRESS_MIN = 3;
const uint8_t LONG_PRESS_MIN_SHTDWN = 120;
const uint8_t GEEK_BOOT_BUTTON_PIN = 0;
const uint8_t X_START_ALL = 10;
const uint8_t Y_START_KBD = 30;
const uint8_t X_STEP_KBD = 10;
const uint8_t Y_STEP_KBD = 14;
const uint8_t Y_START_PWD = Y_START_KBD + 65;
const uint8_t X_START_HLP = 1;
const uint8_t Y_START_HLP = 120;


uint8_t checkLongPress(){
    uint8_t iter = 0;
    while (digitalRead(GEEK_BOOT_BUTTON_PIN) == LOW && iter < LONG_PRESS_MIN_SHTDWN ){
        delay(100);
        iter++;
    }
    return iter;
}

uint8_t readButtons(){
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

void tick() {

    int buttons = readButtons();
    if ( buttons != lastButton){
        if (buttons == BTN_UP) {
            lastInput = KEY_ARROW_LEFT;
        } else if (buttons == (BTN_UP + BTN_LONG)) {
            lastInput = KEY_OK;
        }
        lastButton = buttons;
    }
}

char readChar() {
    tick();
    char c = lastInput;
    lastInput = KEY_NONE;
    return c;
}

char handler() {
    while (true) {
        char c = readChar();
        if (c != KEY_NONE) return c;
        delay(5);
    }
}
//=====================================================

Preferences& getPreferences() {
    static Preferences preferences;
    return preferences;
}

bool loadWifiCredentials(String& ssid, String& password) {
    Preferences& preferences = getPreferences();
    preferences.begin("wifi_settings", true);
    ssid = preferences.getString(NVS_SSID_KEY, "");
    password = preferences.getString(NVS_PASS_KEY, "");
    preferences.end();
    return !ssid.isEmpty() && !password.isEmpty();
}

void setWifiCredentials(String ssid, String password) {
    Preferences& preferences = getPreferences();
    preferences.begin("wifi_settings", false);
    preferences.putString(NVS_SSID_KEY, ssid);
    preferences.putString(NVS_PASS_KEY, password);
    preferences.end();
}

void drawWifiBox(uint16_t borderColor, const String& msg1, const String& msg2, uint8_t textSize = 2) {
    if (!g_tft) return;
    auto& tft = *g_tft;

    tft.fillScreen(TFT_BLACK);
//    tft.setTextFont(1);

    tft.fillRoundRect(20, 20, tft.width() - 40, tft.height() - 40, 5, DARK_GREY);
    tft.drawRoundRect(20, 20, tft.width() - 40, tft.height() - 40, 5, borderColor);

    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(textSize);
    tft.setTextFont(1);
    tft.drawString(msg1, 37, 45);

    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY);
    tft.drawString(msg2, 37, 70);
}

String selectWifiNetwork() {
    if (!g_tft) return "";
    auto& tft = *g_tft;

    int networksCount = 0;

    while (networksCount == 0) {
        drawWifiBox(TFT_GREEN, "Scanning WiFi...", "");
        networksCount = WiFi.scanNetworks();

        if (networksCount == 0) {
            drawWifiBox(TFT_RED, "No networks found", "Retrying...");
            delay(2000);
        }
    }

    int selected = 0;

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(2);
    tft.setTextFont(1);
    tft.drawString("Select Wi-Fi:", 10, 10);

    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(HELP_COLOR, TFT_BLACK);
    tft.drawString("Short press BOOT to change SSID", 10, tft.height() - 35);
    tft.drawString("Long press BOOT to accept", 10, tft.height() - 20);

    while (true) {
        tft.fillRect(0, 40, tft.width(), tft.height() - 80, TFT_BLACK);

        tft.setTextSize(1);
        tft.setTextFont(1);

        int shown = (networksCount < 5) ? networksCount : 5;

        for (int i = 0; i < shown; ++i) {
            String ssid = WiFi.SSID(i);
            if (i == selected) {
                tft.setTextColor(TFT_GREEN);
                tft.drawString("> " + ssid, 20, 40 + i * 15);
            } else {
                tft.setTextColor(TFT_LIGHTGREY);
                tft.drawString("  " + ssid, 20, 40 + i * 15);
            }
        }

        char key = handler();
        if (key == KEY_ARROW_LEFT) {
            selected = (selected + 1) % networksCount;
        } else if (key == KEY_OK) {
            return WiFi.SSID(selected);
        }
    }
}

String enterText(const String& label) {
    if (!g_tft) return "";
    auto& tft = *g_tft;

    const String charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.@!#$%^&*()=+{}[]|\\:;\"'<>,?/~\x08\x0D";

    int index = 0;
    int lastIndex = 0;
    String text = "";

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(2);
    tft.setTextFont(1);
    tft.drawString(label, 10, 10);

    tft.setTextColor(HELP_COLOR);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.drawString("BOOT: short = next, long = select", X_START_HLP, Y_START_HLP);
    tft.drawString("Use saved Wi-Fi or AP setup for easier input", X_START_HLP, Y_START_HLP + 14);
    tft.drawString("[<-] : delete a char. [OK] Enter", X_START_HLP, Y_START_HLP + 28);

    // draw charset on screen
    tft.setTextColor(TFT_GREEN);
    int nbChar = charset.length();
    for(int i = 0, line = 0; i < nbChar; i++){
        if((i>0) && (i%26 == 0)){line++;}
        if (i == nbChar - 1){
            tft.drawString(String("OK"), X_STEP_KBD * (i%26) + X_START_ALL + 2, Y_STEP_KBD * line + Y_START_KBD);
        } else if (i == nbChar - 2){
            tft.drawString(String("<-"), X_STEP_KBD * (i%26) + X_START_ALL, Y_STEP_KBD * line + Y_START_KBD);
        } else {
            tft.drawChar(charset[i], X_STEP_KBD * (i%26) + X_START_ALL, Y_STEP_KBD * line + Y_START_KBD);
        }
    }

    while (true) {
        char currentChar = charset[index];

        tft.setTextSize(1);
        tft.setTextFont(2);

        tft.setTextColor(TFT_GREEN);
        if (lastIndex == nbChar - 1){
            tft.drawString(String("OK"), X_STEP_KBD * (lastIndex%26) + X_START_ALL + 2, Y_STEP_KBD * (int)(lastIndex / 26) + Y_START_KBD);
        } else if (lastIndex == nbChar - 2){
            tft.drawString(String("<-"), X_STEP_KBD * (lastIndex%26) + X_START_ALL, Y_STEP_KBD * (int)(lastIndex / 26) + Y_START_KBD);
        } else {
            tft.drawChar(charset[lastIndex], X_STEP_KBD * (lastIndex%26) + X_START_ALL, Y_STEP_KBD * (int)(lastIndex / 26) + Y_START_KBD);
        }
        tft.setTextColor(TFT_RED);
        if (index == nbChar - 1){
            tft.drawString(String("OK"), X_STEP_KBD * (index % 26) + X_START_ALL + 2, Y_STEP_KBD * (int)(index / 26) + Y_START_KBD);
        } else if (index == nbChar - 2){
            tft.drawString(String("<-"), X_STEP_KBD * (index % 26) + X_START_ALL, Y_STEP_KBD * (int)(index / 26) + Y_START_KBD);
        } else {
            tft.drawChar(charset[index], X_STEP_KBD * (index % 26) + X_START_ALL, Y_STEP_KBD * (int)(index / 26) + Y_START_KBD);
        }

        lastIndex = index;

        tft.setTextColor(TFT_WHITE);

        char key = handler();

        if (key == KEY_ARROW_LEFT) {
            index = (index + 1) % charset.length();
        } else if (key == KEY_OK) {
            if (currentChar == '\x08') {
                if (!text.isEmpty()) text.remove(text.length() - 1);
            } else if (currentChar == '\x0D') {
                return text;
            } else {
                text += currentChar;
            }
            tft.fillRect(0,Y_START_PWD - 2, tft.width(), 25, TFT_BLACK);
            //tft.setTextColor(TFT_LIGHTGREY);
            tft.drawString(text, X_START_ALL, Y_START_PWD);
        }
    }
}

// ---------------- Public entry ----------------
bool setupWaveshareS3GeekWifi(IDeviceView& view) {
  // bind screen pointer once
    g_tft = static_cast<lgfx::LGFX_Device*>(view.getScreen());
    if (!g_tft) return false;

    auto& tft = *g_tft;
    tft.setRotation(3);
    String selectedSsid, ssid, password;

    while (true) {
        selectedSsid = selectWifiNetwork();
        bool hasSavedCredentials = loadWifiCredentials(ssid, password);

        if (!hasSavedCredentials || ssid != selectedSsid) {
            ssid = selectedSsid;
            password = enterText("Password for:\n" + selectedSsid);
        }

        drawWifiBox(TFT_GREEN, "Connecting", ssid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());

        tft.setTextColor(TFT_LIGHTGREY);
        tft.setCursor(55, 100);

        for (int i = 0; i < 30; ++i) {
          if (WiFi.status() == WL_CONNECTED) {
            setWifiCredentials(ssid, password);
            drawWifiBox(TFT_GREEN, "Connected", "");
            delay(2000);
            return true;
          }
          delay(600);
          tft.print(".");
        }

        drawWifiBox(TFT_WHITE, "Failed to connect", "Retry or setup Wi-Fi with Serial");
        setWifiCredentials("", ""); // reset
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        delay(3000);
    }

    return false;
}


#endif
