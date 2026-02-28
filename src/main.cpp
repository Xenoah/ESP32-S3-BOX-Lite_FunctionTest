#include <Arduino.h>
#include <SPI.h>

namespace {

constexpr uint16_t kScreenWidth = 320;
constexpr uint16_t kScreenHeight = 240;

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_WHITE = 0xFFFF;
constexpr uint16_t COLOR_BLUE = 0x001F;
constexpr uint16_t COLOR_CYAN = 0x07FF;
constexpr uint16_t COLOR_YELLOW = 0xFFE0;

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
  digitalWrite(TFT_BL, HIGH);

  lcdSpi.begin(TFT_CLK, TFT_MISO, TFT_MOSI, TFT_CS);
  lcdSpi.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));

  digitalWrite(TFT_RST, HIGH);
  delay(50);
  digitalWrite(TFT_RST, LOW);
  delay(50);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  writeCommand(0x01);
  delay(150);
  writeCommand(0x11);
  delay(120);

  writeCommand(0x3A);
  writeDataByte(0x55);

  writeCommand(0x36);
  writeDataByte(0x60);

  writeCommand(0x21);

  writeCommand(0x13);
  writeCommand(0x29);
  delay(20);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Init display");

  initDisplay();

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
