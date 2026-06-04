#if defined(DEVICE_TDISPLAYS3)

#include "TdisplayDeviceView.h"
#include "Data/WelcomeScreen.h"

TdisplayDeviceView::TdisplayDeviceView() {
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);
}

SPIClass& TdisplayDeviceView::getSharedSpiInstance() {
  return sharedSpi;
}

void* TdisplayDeviceView::getScreen() {
  return &tft; 
}

void TdisplayDeviceView::initialize() {
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  tft.init();
  tft.setRotation(3);
  tft.setSwapBytes(true);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  setBrightness(brightnessPct);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void TdisplayDeviceView::logo() {
  clear();

  // Logo
  tft.setSwapBytes(true);
  tft.pushImage(40, 30, WELCOME_IMAGE_WIDTH, WELCOME_IMAGE_HEIGHT, WelcomeScreen);
  tft.setSwapBytes(false);

  // Sub
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  GlobalState& state = GlobalState::getInstance();
  auto version = std::string("ESP32 Bit Pirate - ") + state.getVersion();
  drawCenterText(version.c_str(), 130, 2);
}

void TdisplayDeviceView::welcome(TerminalTypeEnum& terminalType, std::string& terminalInfos) {
  if (terminalType == TerminalTypeEnum::WiFiAp) welcomeHotspot(terminalInfos);
  else if (terminalType == TerminalTypeEnum::WiFiClient) welcomeWeb(terminalInfos);
  else welcomeSerial(terminalInfos);
}

void TdisplayDeviceView::loading() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(1);

  tft.fillRoundRect(20, 20, tft.width() - 40, tft.height() - 40, 5, DARK_GREY_RECT);
  tft.drawRoundRect(20, 20, tft.width() - 40, tft.height() - 40, 5, TFT_GREEN);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("Loading...", 102, 52);
}

void TdisplayDeviceView::clear() {
  tft.fillScreen(TFT_BLACK);
}

void TdisplayDeviceView::drawLogicTrace(uint8_t pin, const std::vector<uint8_t>& buffer, uint8_t step) {
  tft.fillRect(0, 35, tft.width(), tft.height() - 35, TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print("GPIO ");
  tft.print(pin);

  const int traceY = 50;
  const int traceH = 80;
  const int centerY = traceY + traceH / 2;

  int x = 10;
  for (size_t i = 1; i < buffer.size(); ++i) {
    uint8_t prev = buffer[i - 1];
    uint8_t curr = buffer[i];

    int y1 = prev ? (centerY - 15) : (centerY + 15);
    int y2 = curr ? (centerY - 15) : (centerY + 15);

    if (curr != prev) {
      tft.drawLine(x, y1, x + step, y1, prev ? TFT_GREEN : TFT_WHITE);
      tft.drawLine(x + step, y1, x + step, y2, curr ? TFT_GREEN : TFT_WHITE);
    } else {
      tft.drawLine(x, y1, x + step, y2, curr ? TFT_GREEN : TFT_WHITE);
    }

    x += step;
    if (x > tft.width() - step) break;
  }
}

void TdisplayDeviceView::drawAnalogicTrace(uint8_t pin, const std::vector<uint8_t>& buffer, uint8_t step) {
  tft.fillRect(0, 35, tft.width(), tft.height() - 35, TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print("GPIO ");
  tft.print(pin);

  const int topY = 35;
  const int h = 135;

  int x = 10;
  for (size_t i = 1; i < buffer.size(); ++i) {
    int prev = topY + (h - 1) - (buffer[i - 1] >> 1);
    int curr = topY + (h - 1) - (buffer[i] >> 1);
    tft.drawLine(x, prev, x + step, curr, TFT_GREEN);
    x += step;
    if (x > tft.width() - step) break;
  }
}

void TdisplayDeviceView::drawWaterfall(
    const std::string& title,
    float startValue,
    float endValue,
    const char* unit,
    int rowIndex,
    int rowCount,
    int level
) {
  const int W = tft.width();
  const int H = tft.height();
  const int midX = W / 2;

  const int headerH = 12;
  const int footerH = 12;
  const int graphY  = headerH;
  const int graphH  = H - headerH - footerH;

  const int barMaxPixels = midX - 2;

  // Clamp level
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  int barPixels = (level * barMaxPixels) / 100;

  // First row: titles, labels
  if (rowIndex == 0) {
    // Full clear
    tft.fillScreen(TFT_BLACK);

    // Title
    tft.setTextSize(1);
    tft.setTextFont(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(2, 2);
    tft.print(title.c_str());

    // Format labels
    char bufStart[24];
    char bufEnd[24];

    if (unit && unit[0]) {
      snprintf(bufStart, sizeof(bufStart), "%.2f%s", startValue, unit);
      snprintf(bufEnd,   sizeof(bufEnd),   "%.2f%s", endValue,   unit);
    } else {
      snprintf(bufStart, sizeof(bufStart), "%.2f", startValue);
      snprintf(bufEnd,   sizeof(bufEnd),   "%.2f", endValue);
    }

    // Start label (top right)
    int wStart = tft.textWidth(bufStart);
    tft.setCursor(W - wStart - 2, 2);
    tft.print(bufStart);

    // End label (bottom right)
    int wEnd = tft.textWidth(bufEnd);
    tft.setCursor(W - wEnd - 2, H - footerH + 2);
    tft.print(bufEnd);

    // Graph area
    tft.fillRect(0, graphY, W, graphH, TFT_BLACK);
    tft.drawFastVLine(midX, graphY, graphH, TFT_DARKGREY);
  }

  if (rowCount <= 1) return;
  if (rowIndex < 0) rowIndex = 0;
  if (rowIndex > rowCount - 1) rowIndex = rowCount - 1;

  // Map row to Y
  int y = graphY + (int)((int64_t)rowIndex * (graphH - 1) / (rowCount - 1));

  // Clear this row only
  tft.drawFastHLine(0, y, W, TFT_BLACK);

  // Restore center pixel
  tft.drawPixel(midX, y, TFT_DARKGREY);

  // Draw energy bar
  if (barPixels > 0) {
    int x0 = midX - barPixels;
    int w  = barPixels * 2;

    if (x0 < 0) { w += x0; x0 = 0; }
    if (x0 + w > W) w = W - x0;

    if (w > 0) {
      tft.drawFastHLine(x0, y, w, TFT_GREEN);
    }
  }
}

void TdisplayDeviceView::setRotation(uint8_t rotation) {
  tft.setRotation(rotation);
}

void TdisplayDeviceView::setBrightness(uint8_t brightness) {
  if (brightness > 100) brightness = 100;
  brightnessPct = brightness;

  uint8_t pwm = (uint8_t)((brightnessPct * 255) / 100);
  tft.setBrightness(pwm);
}

uint8_t TdisplayDeviceView::getBrightness() {
  return brightnessPct;
}

void TdisplayDeviceView::topBar(const std::string& title, bool submenu, bool searchBar) {
  (void)submenu;
  (void)searchBar;

  // Zone topbar
  tft.fillRect(0, 0, tft.width(), 30, TFT_BLACK);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(title.c_str(), tft.width() / 2, 20);
  tft.setTextDatum(TL_DATUM);
}

void TdisplayDeviceView::horizontalSelection(
  const std::vector<std::string>& options,
  uint16_t selectedIndex,
  const std::string& description1,
  const std::string& description2
) {
  const int originY = 30;

  // Box option
  const std::string& option = options[selectedIndex];
  int boxX = 60;
  int boxW = tft.width() - 120;
  int boxY = originY + 45;
  int boxH = 50;
  int corner = 8;

  // Description 1
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(description1.c_str(), tft.width() / 2, originY + 26);

  // Description 2 
  tft.setTextColor(HELP_COLOR, TFT_BLACK);
  tft.drawString("Short press any button to change terminal", tft.width() / 2, tft.height() - 30);
  tft.drawString("Long press top button to accept", tft.width() / 2, tft.height() - 12);

  // Box background + border
  tft.fillRoundRect(boxX, boxY, boxW, boxH, corner, DARK_GREY_RECT);
  tft.drawRoundRect(boxX, boxY, boxW, boxH, corner, TFT_GREEN);

  const int pad = 4;  
  int innerX = boxX + pad;
  int innerY = boxY + pad;
  int innerW = boxW - (pad * 2);
  int innerH = boxH - (pad * 2);
  int innerCorner = corner - pad;
  if (innerCorner < 0) innerCorner = 0;

  // erase old text
  tft.fillRoundRect(innerX, innerY, innerW, innerH, innerCorner, DARK_GREY_RECT);

  // Option name
  tft.setTextColor(TFT_WHITE, DARK_GREY_RECT);
  tft.setTextFont(2);
  tft.setTextSize(2);

  int textW = tft.textWidth(option.c_str());
  int textX = (tft.width() - textW) / 2;
  int textH = tft.fontHeight();
  int textY = boxY + (boxH - textH) / 2;

  tft.setTextDatum(TL_DATUM);
  tft.drawString(option.c_str(), textX, textY);

  // Arrows
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(35, boxY + 19);
  tft.print("<");
  tft.setCursor(tft.width() - 40, boxY + 19);
  tft.print(">");
}

void TdisplayDeviceView::drawCenterText(const std::string& text, int y, int fontSize) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(fontSize);
  tft.drawString(text.c_str(), tft.width() / 2, y);
  tft.setTextDatum(TL_DATUM);
}

void TdisplayDeviceView::welcomeSerial(const std::string& baudStr) {
  tft.fillScreen(TFT_BLACK);

  // Titre
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.setCursor(88, 35);
  tft.println("Open Serial (USB COM)");

  // Rect baudrate
  tft.fillRoundRect(70, 60, 180, 40, 8, DARK_GREY_RECT);
  tft.drawRoundRect(70, 60, 180, 40, 8, TFT_GREEN);

  // Texte baud
  std::string baud = "Baudrate: " + baudStr;
  int textW = tft.textWidth(baud.c_str());
  tft.setTextColor(TFT_WHITE, DARK_GREY_RECT);
  tft.setCursor((tft.width() - textW) / 2, 73);
  tft.print(baud.c_str());

  // Sub
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(65, 107);
  tft.println("Then press any key in terminal");
}

void TdisplayDeviceView::welcomeWeb(const std::string& ipStr) {
  tft.fillScreen(TFT_BLACK);

  // Titre
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.setCursor(82, 34);
  tft.println("Open browser to connect");

  // Rectangle IP
  tft.fillRoundRect(60, 60, 200, 40, 8, DARK_GREY_RECT);
  tft.drawRoundRect(60, 60, 200, 40, 8, TFT_GREEN);

  // Texte IP
  std::string ip = "http://" + ipStr;
  int textW = tft.textWidth(ip.c_str());
  tft.setTextColor(TFT_WHITE, DARK_GREY_RECT);
  tft.setCursor((tft.width() - textW) / 2, 73);
  tft.print(ip.c_str());

}

void TdisplayDeviceView::welcomeHotspot(const std::string& ipStr) {
  GlobalState& state = GlobalState::getInstance();
  PinoutConfig config;
  config.setMode("HOTSPOT");
  config.setMappings({
    state.getActiveApName(),
    std::string("PW ") + state.getApPassword(),
    std::string("IP ") + ipStr,
    "CONNECT TO AP"
  });
  show(config);
}

void TdisplayDeviceView::adapterMode(const std::string& adapterName, const std::string& description, const std::vector<std::string>& details) {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(adapterName.c_str(), tft.width() / 2, 28);

  tft.setTextSize(1);
  size_t detailCount = std::min<size_t>(details.size(), 8);
  size_t detailRows = (detailCount + 1) / 2;
  for (size_t i = 0; i < detailCount; ++i) {
    int col = i % 2;
    int row = i / 2;
    int boxX = 30 + (col * 135);
    int boxY = 56 + (row * 18);
    int boxW = 125;
    tft.fillRoundRect(boxX, boxY, boxW, 15, 5, DARK_GREY_RECT);
    tft.drawRoundRect(boxX, boxY, boxW, 15, 5, TFT_GREEN);
    tft.setTextColor(TFT_WHITE, DARK_GREY_RECT);
    tft.drawString(details[i].c_str(), boxX + (boxW / 2), boxY + 8);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int descY = detailRows > 2 ? 135 : 112;
  tft.drawString(description.c_str(), tft.width() / 2, descY);
  tft.setTextColor(HELP_COLOR, TFT_BLACK);
  tft.drawString("Press any button to return", tft.width() / 2, descY + 30);
  tft.setTextDatum(TL_DATUM);
}

void TdisplayDeviceView::shutDown() {
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  drawCenterText("Shutting down...", 80, 2);
  delay(1000);

  // backlight + power
  digitalWrite(PIN_LCD_BL, LOW);
  digitalWrite(PIN_POWER_ON, LOW);
}

void TdisplayDeviceView::show(PinoutConfig& config) {
  tft.fillScreen(TFT_BLACK);

  const auto& mappings = config.getMappings();
  auto mode = config.getMode();

  // Mode name
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  std::string modeStr = "MODE " + mode;
  tft.drawString(modeStr.c_str(), tft.width() / 2, 20);
  tft.setTextDatum(TL_DATUM);

  // No mapping
  if (mappings.empty()) {
    const int frameX = 20;
    const int frameY = 45;
    const int frameW = tft.width() - 40;
    const int frameH = tft.height() - 70;
    const int frameR = 5;

    tft.fillRoundRect(frameX, frameY, frameW, frameH, frameR, TFT_BLACK);
    tft.drawRoundRect(frameX, frameY, frameW, frameH, frameR, TFT_GREEN);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Nothing to display", tft.width() / 2, frameY + frameH / 2);
    tft.setTextDatum(TL_DATUM);
    return;
  }

  // Mapping list
  int boxHeight = 24;
  int startY = 40;

  for (size_t i = 0; i < mappings.size(); ++i) {
    int y = startY + (int)i * (boxHeight + 4);

    tft.fillRoundRect(20, y, tft.width() - 40, boxHeight, 6, DARK_GREY_RECT);
    tft.drawRoundRect(20, y, tft.width() - 40, boxHeight, 6, TFT_GREEN);

    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, DARK_GREY_RECT);

    int w = tft.textWidth(mappings[i].c_str());
    int textX = (tft.width() - w) / 2;

    tft.setCursor(textX, y + 5);
    tft.print(mappings[i].c_str());
  }
}

#endif
