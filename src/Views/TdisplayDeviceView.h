#pragma once

#if defined(DEVICE_TDISPLAYS3)

#include "Interfaces/IDeviceView.h"
#include "States/GlobalState.h"

#include <Arduino.h>
#include <LovyanGFX.hpp>


#define PIN_LCD_BL 38
#define PIN_POWER_ON 46
#define DARK_GREY_RECT 0x4208
#define HELP_COLOR 0xC618

// Lovyan driver
class LGFX_Tdisplay : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel;
  lgfx::Bus_Parallel8 _bus;
  lgfx::Light_PWM     _light;

public:
  LGFX_Tdisplay() {
    {
    auto cfg = _bus.config();

      //cfg.i2s_port = I2S_NUM_0;     // I2S_NUM_0 or I2S_NUM_1
      cfg.freq_write = 20000000;    // 20MHz, 80MHz
      cfg.pin_wr =  8;              // WR
      cfg.pin_rd =  9;              // RD
      cfg.pin_rs =  7;              // RS(D/C)
      cfg.pin_d0 = 39;              // D0
      cfg.pin_d1 = 40;              // D1
      cfg.pin_d2 = 41;              // D2
      cfg.pin_d3 = 42;              // D3
      cfg.pin_d4 = 45;              // D4
      cfg.pin_d5 = 46;              // D5
      cfg.pin_d6 = 47;              // D6
      cfg.pin_d7 = 48;              // D7

    _bus.config(cfg);
    _panel.setBus(&_bus);
    }

    // --- PANEL
    {
    auto cfg = _panel.config();


    cfg.pin_cs   = 6;
    cfg.pin_rst  = 5;
    cfg.pin_busy = -1;

    cfg.panel_width  = 170;
    cfg.panel_height = 320;

    // VRAM interne 240x320
    cfg.memory_width  = 240;
    cfg.memory_height = 320;
    cfg.offset_x = 35;
    cfg.offset_y = 0;
    
    cfg.invert = true;
    cfg.rgb_order = false;  // RGB

    cfg.dlen_16bit = false;
    _panel.config(cfg);
    }

    setPanel(&_panel);
  }
};

class TdisplayDeviceView : public IDeviceView {
public:
  TdisplayDeviceView();

  void initialize() override;
  SPIClass& getSharedSpiInstance() override;
  void* getScreen() override;
  void logo() override;
  void welcome(TerminalTypeEnum& terminalType, std::string& terminalInfos) override;
  void show(PinoutConfig& config) override;
  void loading() override;
  void adapterMode(const std::string& adapterName, const std::string& description, const std::vector<std::string>& details) override;
  void clear() override;
  void drawLogicTrace(uint8_t pin, const std::vector<uint8_t>& buffer, uint8_t step) override;
  void drawAnalogicTrace(uint8_t pin, const std::vector<uint8_t>& buffer, uint8_t step) override;
  void drawWaterfall(const std::string& title, float startValue, float endValue, const char* unit, int rowIndex, int rowCount, int level) override;
  void setRotation(uint8_t rotation) override;
  void setBrightness(uint8_t brightness) override;
  uint8_t getBrightness() override;
  void topBar(const std::string& title, bool submenu, bool searchBar) override;
  void horizontalSelection(
    const std::vector<std::string>& options,
    uint16_t selectedIndex,
    const std::string& description1,
    const std::string& description2
  ) override;

  void shutDown();

  
private:
  LGFX_Tdisplay tft;
  //   lgfx::LGFX_Sprite canvas; // Not used currently, memory consumption is too high
  uint8_t brightnessPct = 100;
  SPIClass sharedSpi{HSPI}; // or FSPI

  void drawCenterText(const std::string& text, int y, int fontSize);
  void welcomeWeb(const std::string& ip);
  void welcomeSerial(const std::string& baud);
};

#endif
