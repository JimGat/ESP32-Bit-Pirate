#pragma once

#if defined(DEVICE_WAVESHARE_S3_GEEK)

#include "Interfaces/IDeviceView.h"
#include "States/GlobalState.h"

#include <Arduino.h>
#include <LovyanGFX.hpp>


#define GEEK_PIN_LCD_BL 7
#define DARK_GREY_RECT 0x4208
#define HELP_COLOR 0xC618

// Lovyan driver
class LGFX_WaveshareS3Geek : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

public:
  LGFX_WaveshareS3Geek() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI3_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 12;
      cfg.pin_mosi = 11;
      cfg.pin_miso = -1;
      cfg.pin_dc = 8;

      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    {
      auto cfg = _panel.config();

      cfg.pin_cs = 10;
      cfg.pin_rst = 9;
      cfg.pin_busy = -1;

      cfg.panel_width = 135;
      cfg.panel_height = 240;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.offset_x = 52;
      cfg.offset_y = 40;
      cfg.offset_rotation = 0;

      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;

      _panel.config(cfg);
    }

    {
      auto cfg = _light.config();
      cfg.pin_bl = GEEK_PIN_LCD_BL;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;

      _light.config(cfg);
      _panel.setLight(&_light);
    }

    setPanel(&_panel);
  }
};

class WaveshareS3GeekDeviceView : public IDeviceView {
public:
  WaveshareS3GeekDeviceView();

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
  LGFX_WaveshareS3Geek tft;
  //   lgfx::LGFX_Sprite canvas; // Not used currently, memory consumption is too high
  uint8_t brightnessPct = 100;
  SPIClass sharedSpi{HSPI}; // or FSPI

  void drawCenterText(const std::string& text, int y, int fontSize);
  void welcomeWeb(const std::string& ip);
  void welcomeHotspot(const std::string& ip);
  void welcomeSerial(const std::string& baud);
};

#endif
