#include <Arduino.h>
#include <SPI.h>

namespace {

constexpr uint16_t kScreenWidth = 320;
constexpr uint16_t kScreenHeight = 240;

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_RED = 0xF800;
constexpr uint16_t COLOR_GREEN = 0x07E0;
constexpr uint16_t COLOR_WHITE = 0xFFFF;
constexpr uint16_t COLOR_BLUE = 0x001F;
constexpr uint16_t COLOR_CYAN = 0x07FF;
constexpr uint16_t COLOR_YELLOW = 0xFFE0;
constexpr uint8_t kBacklightOnLevel = LOW;

SPIClass lcdSpi(FSPI);

struct Glyph {
  char ch;
  uint8_t rows[7];
};

constexpr Glyph kGlyphs[] = {
    {' ', {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000}},
    {'-', {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000}},
    {'3', {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'B', {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},
    {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
    {'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'I', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b11111}},
    {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
    {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'X', {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},
};

const Glyph *findGlyph(char ch) {
  for (const auto &glyph : kGlyphs) {
    if (glyph.ch == ch) {
      return &glyph;
    }
  }
  return &kGlyphs[0];
}

void writeCommand(uint8_t command) {
  digitalWrite(TFT_DC, LOW);
  digitalWrite(TFT_CS, LOW);
  lcdSpi.write(command);
  digitalWrite(TFT_CS, HIGH);
}

void writeDataByte(uint8_t data) {
  digitalWrite(TFT_DC, HIGH);
  digitalWrite(TFT_CS, LOW);
  lcdSpi.write(data);
  digitalWrite(TFT_CS, HIGH);
}

void writeData(const uint8_t *data, size_t length) {
  digitalWrite(TFT_DC, HIGH);
  digitalWrite(TFT_CS, LOW);
  lcdSpi.writeBytes(data, length);
  digitalWrite(TFT_CS, HIGH);
}

void writeCommandWithData(uint8_t command, const uint8_t *data, size_t length) {
  writeCommand(command);
  writeData(data, length);
}

void setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  uint8_t data[4];

  writeCommand(0x2A);
  data[0] = x0 >> 8;
  data[1] = x0 & 0xFF;
  data[2] = x1 >> 8;
  data[3] = x1 & 0xFF;
  writeData(data, sizeof(data));

  writeCommand(0x2B);
  data[0] = y0 >> 8;
  data[1] = y0 & 0xFF;
  data[2] = y1 >> 8;
  data[3] = y1 & 0xFF;
  writeData(data, sizeof(data));

  writeCommand(0x2C);
}

void sendColor(uint16_t color, uint32_t count) {
  uint8_t buffer[128];
  for (size_t i = 0; i < sizeof(buffer); i += 2) {
    buffer[i] = color >> 8;
    buffer[i + 1] = color & 0xFF;
  }

  digitalWrite(TFT_DC, HIGH);
  digitalWrite(TFT_CS, LOW);
  while (count > 0) {
    const uint32_t chunkPixels = min<uint32_t>(count, sizeof(buffer) / 2);
    lcdSpi.writeBytes(buffer, chunkPixels * 2);
    count -= chunkPixels;
  }
  digitalWrite(TFT_CS, HIGH);
}

void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  if (x >= kScreenWidth || y >= kScreenHeight || w == 0 || h == 0) {
    return;
  }
  if (x + w > kScreenWidth) {
    w = kScreenWidth - x;
  }
  if (y + h > kScreenHeight) {
    h = kScreenHeight - y;
  }

  setAddressWindow(x, y, x + w - 1, y + h - 1);
  sendColor(color, static_cast<uint32_t>(w) * h);
}

void fillScreen(uint16_t color) {
  fillRect(0, 0, kScreenWidth, kScreenHeight, color);
}

void drawGlyph(int16_t x, int16_t y, char ch, uint8_t scale, uint16_t fg, uint16_t bg) {
  const Glyph *glyph = findGlyph(ch);
  for (uint8_t row = 0; row < 7; ++row) {
    for (uint8_t col = 0; col < 5; ++col) {
      const bool on = glyph->rows[row] & (1 << (4 - col));
      fillRect(x + col * scale, y + row * scale, scale, scale, on ? fg : bg);
    }
  }
}

void drawTextCentered(int16_t y, const char *text, uint8_t scale, uint16_t fg, uint16_t bg) {
  const size_t len = strlen(text);
  const int16_t charWidth = 6 * scale;
  const int16_t textWidth = static_cast<int16_t>(len) * charWidth - scale;
  int16_t x = (kScreenWidth - textWidth) / 2;

  for (size_t i = 0; i < len; ++i) {
    drawGlyph(x, y, text[i], scale, fg, bg);
    x += charWidth;
  }
}

void initDisplay() {
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_BL, OUTPUT);

  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_DC, HIGH);
  digitalWrite(TFT_BL, !kBacklightOnLevel);

  lcdSpi.begin(TFT_CLK, TFT_MISO, TFT_MOSI, TFT_CS);
  lcdSpi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));

  digitalWrite(TFT_RST, HIGH);
  delay(50);
  digitalWrite(TFT_RST, LOW);
  delay(50);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  writeCommand(0x01);
  delay(150);

  const uint8_t porch[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
  writeCommandWithData(0xB2, porch, sizeof(porch));

  const uint8_t gateCtrl[] = {0x35};
  writeCommandWithData(0xB7, gateCtrl, sizeof(gateCtrl));

  const uint8_t vcoms[] = {0x28};
  writeCommandWithData(0xBB, vcoms, sizeof(vcoms));

  const uint8_t lcmCtrl[] = {0x0C};
  writeCommandWithData(0xC0, lcmCtrl, sizeof(lcmCtrl));

  const uint8_t vdvVrhEn[] = {0x01, 0xFF};
  writeCommandWithData(0xC2, vdvVrhEn, sizeof(vdvVrhEn));

  const uint8_t vrhs[] = {0x10};
  writeCommandWithData(0xC3, vrhs, sizeof(vrhs));

  const uint8_t vdvs[] = {0x20};
  writeCommandWithData(0xC4, vdvs, sizeof(vdvs));

  const uint8_t frameRate[] = {0x0F};
  writeCommandWithData(0xC6, frameRate, sizeof(frameRate));

  const uint8_t powerCtrl[] = {0xA4, 0xA1};
  writeCommandWithData(0xD0, powerCtrl, sizeof(powerCtrl));

  const uint8_t positiveGamma[] = {
      0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x32, 0x44,
      0x42, 0x06, 0x0E, 0x12, 0x14, 0x17,
  };
  writeCommandWithData(0xE0, positiveGamma, sizeof(positiveGamma));

  const uint8_t negativeGamma[] = {
      0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x31, 0x54,
      0x47, 0x0E, 0x1C, 0x17, 0x1B, 0x1E,
  };
  writeCommandWithData(0xE1, negativeGamma, sizeof(negativeGamma));

  const uint8_t colorMode[] = {0x55};
  writeCommandWithData(0x3A, colorMode, sizeof(colorMode));

  const uint8_t madctl[] = {0xA0};
  writeCommandWithData(0x36, madctl, sizeof(madctl));

  writeCommand(0x21);
  writeCommand(0x11);
  delay(120);
  writeCommand(0x13);
  writeCommand(0x29);
  delay(50);

  digitalWrite(TFT_BL, kBacklightOnLevel);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Init display");

  initDisplay();

  fillScreen(COLOR_RED);
  delay(250);
  fillScreen(COLOR_GREEN);
  delay(250);
  fillScreen(COLOR_BLUE);
  fillRect(20, 20, kScreenWidth - 40, kScreenHeight - 40, COLOR_BLACK);
  fillRect(30, 30, kScreenWidth - 60, 44, COLOR_CYAN);

  drawTextCentered(42, "HELLO", 5, COLOR_BLACK, COLOR_CYAN);
  drawTextCentered(112, "BOX-LITE", 4, COLOR_WHITE, COLOR_BLACK);
  drawTextCentered(164, "ESP32-S3", 3, COLOR_YELLOW, COLOR_BLACK);

  Serial.println("Text drawn");
}

void loop() {
  delay(1000);
}
