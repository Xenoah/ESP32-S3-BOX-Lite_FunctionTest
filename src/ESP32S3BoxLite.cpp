#include "ESP32S3BoxLite.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "driver/i2c.h"
#include "driver/i2s.h"

namespace {

constexpr int kLcdMosiPin = 6;
constexpr int kLcdClkPin = 7;
constexpr int kLcdCsPin = 5;
constexpr int kLcdDcPin = 4;
constexpr int kLcdRstPin = 48;
constexpr int kLcdBacklightPin = 45;

constexpr int kI2cPort = I2C_NUM_0;
constexpr int kI2cSdaPin = 8;
constexpr int kI2cSclPin = 18;

constexpr int kI2sPort = I2S_NUM_0;
constexpr int kI2sBclkPin = 17;
constexpr int kI2sMclkPin = 2;
constexpr int kI2sWsPin = 47;
constexpr int kI2sDataOutPin = 15;
constexpr int kI2sDataInPin = 16;
constexpr int kPowerAmpPin = 46;

constexpr int kConfigButtonPin = 0;
constexpr int kAdcButtonPin = 1;

constexpr uint8_t kBacklightOnLevel = LOW;
constexpr int kButtonNextMinMv = 720;
constexpr int kButtonNextMaxMv = 920;
constexpr int kButtonEnterMinMv = 1880;
constexpr int kButtonEnterMaxMv = 2080;
constexpr int kButtonPrevMinMv = 2310;
constexpr int kButtonPrevMaxMv = 2510;
constexpr int kDebounceMs = 25;

constexpr uint32_t kAudioSampleRate = 22050;
constexpr float kPi = 3.14159265358979323846f;

constexpr uint8_t kEs8156Address = 0x10;
constexpr uint8_t kEs7243eAddress = 0x20;

struct Glyph {
  char ch;
  uint8_t rows[7];
};

constexpr Glyph kGlyphs[] = {
    {' ', {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000}},
    {'-', {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000}},
    {'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}},
    {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'2', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}},
    {'3', {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}},
    {'5', {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110}},
    {'6', {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}},
    {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}},
    {'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}},
    {'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b11100}},
    {'A', {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'B', {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},
    {'C', {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}},
    {'D', {0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100}},
    {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
    {'F', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'G', {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110}},
    {'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'I', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b11111}},
    {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
    {'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
    {'N', {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001}},
    {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'P', {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
    {'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b01010, 0b00100}},
    {'X', {0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b01010, 0b10001}},
    {'Y', {0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'Z', {0b11111, 0b00010, 0b00100, 0b00100, 0b01000, 0b10000, 0b11111}},
};

const Glyph *findGlyph(char ch) {
  for (const auto &glyph : kGlyphs) {
    if (glyph.ch == ch) {
      return &glyph;
    }
  }
  return &kGlyphs[0];
}

}  // namespace

bool ESP32S3BoxLiteDisplay::begin() {
  pinMode(kLcdCsPin, OUTPUT);
  pinMode(kLcdDcPin, OUTPUT);
  pinMode(kLcdRstPin, OUTPUT);
  pinMode(kLcdBacklightPin, OUTPUT);

  digitalWrite(kLcdCsPin, HIGH);
  digitalWrite(kLcdDcPin, HIGH);
  digitalWrite(kLcdBacklightPin, !kBacklightOnLevel);

  spi_.begin(kLcdClkPin, -1, kLcdMosiPin, kLcdCsPin);
  spi_.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));

  digitalWrite(kLcdRstPin, HIGH);
  delay(50);
  digitalWrite(kLcdRstPin, LOW);
  delay(50);
  digitalWrite(kLcdRstPin, HIGH);
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

  digitalWrite(kLcdBacklightPin, kBacklightOnLevel);
  initialized_ = true;
  return true;
}

void ESP32S3BoxLiteDisplay::writeCommand(uint8_t command) {
  digitalWrite(kLcdDcPin, LOW);
  digitalWrite(kLcdCsPin, LOW);
  spi_.write(command);
  digitalWrite(kLcdCsPin, HIGH);
}

void ESP32S3BoxLiteDisplay::writeData(const uint8_t *data, size_t length) {
  digitalWrite(kLcdDcPin, HIGH);
  digitalWrite(kLcdCsPin, LOW);
  spi_.writeBytes(data, length);
  digitalWrite(kLcdCsPin, HIGH);
}

void ESP32S3BoxLiteDisplay::writeCommandWithData(uint8_t command, const uint8_t *data, size_t length) {
  writeCommand(command);
  writeData(data, length);
}

void ESP32S3BoxLiteDisplay::setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
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

void ESP32S3BoxLiteDisplay::sendColor(uint16_t color, uint32_t count) {
  uint8_t buffer[128];
  for (size_t i = 0; i < sizeof(buffer); i += 2) {
    buffer[i] = color >> 8;
    buffer[i + 1] = color & 0xFF;
  }

  digitalWrite(kLcdDcPin, HIGH);
  digitalWrite(kLcdCsPin, LOW);
  while (count > 0) {
    const uint32_t chunkPixels = std::min<uint32_t>(count, sizeof(buffer) / 2);
    spi_.writeBytes(buffer, chunkPixels * 2);
    count -= chunkPixels;
  }
  digitalWrite(kLcdCsPin, HIGH);
}

void ESP32S3BoxLiteDisplay::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  if (!initialized_ || x >= Width || y >= Height || w == 0 || h == 0) {
    return;
  }
  if (x + w > Width) {
    w = Width - x;
  }
  if (y + h > Height) {
    h = Height - y;
  }

  setAddressWindow(x, y, x + w - 1, y + h - 1);
  sendColor(color, static_cast<uint32_t>(w) * h);
}

void ESP32S3BoxLiteDisplay::fillScreen(uint16_t color) {
  fillRect(0, 0, Width, Height, color);
}

void ESP32S3BoxLiteDisplay::drawGlyph(int16_t x, int16_t y, char ch, uint8_t scale, uint16_t fg, uint16_t bg) {
  const Glyph *glyph = findGlyph(ch);
  for (uint8_t row = 0; row < 7; ++row) {
    for (uint8_t col = 0; col < 5; ++col) {
      const bool on = glyph->rows[row] & (1 << (4 - col));
      fillRect(x + col * scale, y + row * scale, scale, scale, on ? fg : bg);
    }
  }
}

void ESP32S3BoxLiteDisplay::drawText(int16_t x, int16_t y, const char *text, uint8_t scale, uint16_t fg, uint16_t bg) {
  const int16_t charWidth = 6 * scale;
  for (size_t i = 0; text[i] != '\0'; ++i) {
    drawGlyph(x, y, text[i], scale, fg, bg);
    x += charWidth;
  }
}

void ESP32S3BoxLiteDisplay::drawTextCentered(int16_t y, const char *text, uint8_t scale, uint16_t fg, uint16_t bg) {
  const size_t len = strlen(text);
  const int16_t charWidth = 6 * scale;
  const int16_t textWidth = static_cast<int16_t>(len) * charWidth - scale;
  const int16_t x = (Width - textWidth) / 2;
  drawText(x, y, text, scale, fg, bg);
}

bool ESP32S3BoxLiteInput::begin() {
  pinMode(kConfigButtonPin, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(kAdcButtonPin, ADC_11db);
  return true;
}

ESP32S3BoxLiteButton ESP32S3BoxLiteInput::readRawButton() {
  if (digitalRead(kConfigButtonPin) == LOW) {
    lastAdcMilliVolts_ = 0;
    return ESP32S3BoxLiteButton::Config;
  }

  lastAdcMilliVolts_ = static_cast<int>(analogReadMilliVolts(kAdcButtonPin));
  if (lastAdcMilliVolts_ >= kButtonPrevMinMv && lastAdcMilliVolts_ <= kButtonPrevMaxMv) {
    return ESP32S3BoxLiteButton::Prev;
  }
  if (lastAdcMilliVolts_ >= kButtonEnterMinMv && lastAdcMilliVolts_ <= kButtonEnterMaxMv) {
    return ESP32S3BoxLiteButton::Enter;
  }
  if (lastAdcMilliVolts_ >= kButtonNextMinMv && lastAdcMilliVolts_ <= kButtonNextMaxMv) {
    return ESP32S3BoxLiteButton::Next;
  }
  return ESP32S3BoxLiteButton::None;
}

ESP32S3BoxLiteButton ESP32S3BoxLiteInput::pollButtonEvent() {
  const uint32_t now = millis();
  const ESP32S3BoxLiteButton sample = readRawButton();

  if (sample != lastSample_) {
    lastSample_ = sample;
    debounceStartedMs_ = now;
  }

  if (sample != stableButton_ && (now - debounceStartedMs_) >= kDebounceMs) {
    stableButton_ = sample;
    if (stableButton_ != ESP32S3BoxLiteButton::None) {
      return stableButton_;
    }
  }
  return ESP32S3BoxLiteButton::None;
}

int ESP32S3BoxLiteInput::lastAdcMilliVolts() const {
  return lastAdcMilliVolts_;
}

const char *ESP32S3BoxLiteInput::buttonName(ESP32S3BoxLiteButton button) {
  switch (button) {
    case ESP32S3BoxLiteButton::Config:
      return "CFG";
    case ESP32S3BoxLiteButton::Prev:
      return "PRV";
    case ESP32S3BoxLiteButton::Enter:
      return "ENT";
    case ESP32S3BoxLiteButton::Next:
      return "NXT";
    case ESP32S3BoxLiteButton::None:
    default:
      return "NONE";
  }
}

bool ESP32S3BoxLiteAudio::begin() {
  if (initialized_) {
    return true;
  }

  pinMode(kPowerAmpPin, OUTPUT);
  digitalWrite(kPowerAmpPin, LOW);

  if (!initI2cBus() || !initI2sBus() || !initEs8156() || !startEs8156() || !initEs7243e()) {
    return false;
  }
  if (!setSpeakerVolumePercent(volumePercent_) || !setMicGainDb(micGainDb_)) {
    return false;
  }

  initialized_ = true;
  return true;
}

bool ESP32S3BoxLiteAudio::ready() const {
  return initialized_;
}

uint32_t ESP32S3BoxLiteAudio::sampleRate() const {
  return kAudioSampleRate;
}

bool ESP32S3BoxLiteAudio::initI2cBus() {
  i2c_config_t config = {};
  config.mode = I2C_MODE_MASTER;
  config.sda_io_num = static_cast<gpio_num_t>(kI2cSdaPin);
  config.scl_io_num = static_cast<gpio_num_t>(kI2cSclPin);
  config.sda_pullup_en = GPIO_PULLUP_ENABLE;
  config.scl_pullup_en = GPIO_PULLUP_ENABLE;
  config.master.clk_speed = 100000;

  esp_err_t err = i2c_param_config(static_cast<i2c_port_t>(kI2cPort), &config);
  if (err != ESP_OK) {
    return false;
  }
  err = i2c_driver_install(static_cast<i2c_port_t>(kI2cPort), config.mode, 0, 0, 0);
  return err == ESP_OK || err == ESP_ERR_INVALID_STATE;
}

bool ESP32S3BoxLiteAudio::initI2sBus() {
  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  config.sample_rate = kAudioSampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM;
  config.dma_buf_count = 3;
  config.dma_buf_len = 1024;
  config.use_apll = true;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  esp_err_t err = i2s_driver_install(static_cast<i2s_port_t>(kI2sPort), &config, 0, nullptr);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return false;
  }

  i2s_pin_config_t pins = {};
  pins.mck_io_num = static_cast<gpio_num_t>(kI2sMclkPin);
  pins.bck_io_num = static_cast<gpio_num_t>(kI2sBclkPin);
  pins.ws_io_num = static_cast<gpio_num_t>(kI2sWsPin);
  pins.data_out_num = static_cast<gpio_num_t>(kI2sDataOutPin);
  pins.data_in_num = static_cast<gpio_num_t>(kI2sDataInPin);

  err = i2s_set_pin(static_cast<i2s_port_t>(kI2sPort), &pins);
  if (err != ESP_OK) {
    return false;
  }

  err = i2s_zero_dma_buffer(static_cast<i2s_port_t>(kI2sPort));
  return err == ESP_OK;
}

bool ESP32S3BoxLiteAudio::writeRegister(uint8_t deviceAddress, uint8_t reg, uint8_t value) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  if (cmd == nullptr) {
    return false;
  }

  esp_err_t err = ESP_OK;
  err |= i2c_master_start(cmd);
  err |= i2c_master_write_byte(cmd, deviceAddress, true);
  err |= i2c_master_write_byte(cmd, reg, true);
  err |= i2c_master_write_byte(cmd, value, true);
  err |= i2c_master_stop(cmd);
  err |= i2c_master_cmd_begin(static_cast<i2c_port_t>(kI2cPort), cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return err == ESP_OK;
}

bool ESP32S3BoxLiteAudio::readRegister(uint8_t deviceAddress, uint8_t reg, uint8_t &value) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  if (cmd == nullptr) {
    return false;
  }

  esp_err_t err = ESP_OK;
  err |= i2c_master_start(cmd);
  err |= i2c_master_write_byte(cmd, deviceAddress, true);
  err |= i2c_master_write_byte(cmd, reg, true);
  err |= i2c_master_stop(cmd);
  err |= i2c_master_cmd_begin(static_cast<i2c_port_t>(kI2cPort), cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  if (err != ESP_OK) {
    return false;
  }

  cmd = i2c_cmd_link_create();
  if (cmd == nullptr) {
    return false;
  }

  err = ESP_OK;
  err |= i2c_master_start(cmd);
  err |= i2c_master_write_byte(cmd, deviceAddress | 0x01, true);
  err |= i2c_master_read_byte(cmd, &value, I2C_MASTER_LAST_NACK);
  err |= i2c_master_stop(cmd);
  err |= i2c_master_cmd_begin(static_cast<i2c_port_t>(kI2cPort), cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return err == ESP_OK;
}

bool ESP32S3BoxLiteAudio::initEs8156() {
  const uint8_t sequence[][2] = {
      {0x02, 0x04}, {0x20, 0x2A}, {0x21, 0x3C}, {0x22, 0x00}, {0x24, 0x07}, {0x23, 0x00},
      {0x0A, 0x01}, {0x0B, 0x01}, {0x11, 0x00}, {0x14, 179},  {0x0D, 0x14}, {0x18, 0x00},
      {0x08, 0x3F}, {0x00, 0x02}, {0x00, 0x03}, {0x25, 0x20},
  };

  for (const auto &item : sequence) {
    if (!writeRegister(kEs8156Address, item[0], item[1])) {
      return false;
    }
  }
  digitalWrite(kPowerAmpPin, HIGH);
  return true;
}

bool ESP32S3BoxLiteAudio::startEs8156() {
  const uint8_t sequence[][2] = {
      {0x08, 0x3F}, {0x09, 0x00}, {0x18, 0x00}, {0x25, 0x20}, {0x22, 0x00},
      {0x21, 0x3C}, {0x19, 0x20}, {0x14, 179},
  };

  for (const auto &item : sequence) {
    if (!writeRegister(kEs8156Address, item[0], item[1])) {
      return false;
    }
  }
  digitalWrite(kPowerAmpPin, HIGH);
  return true;
}

bool ESP32S3BoxLiteAudio::initEs7243e() {
  const uint8_t sequence[][2] = {
      {0x01, 0x3A}, {0x00, 0x80}, {0xF9, 0x00}, {0x04, 0x02}, {0x04, 0x01}, {0xF9, 0x01},
      {0x00, 0x1E}, {0x01, 0x00}, {0x02, 0x00}, {0x03, 0x20}, {0x04, 0x01}, {0x0D, 0x00},
      {0x05, 0x00}, {0x06, 0x03}, {0x07, 0x00}, {0x08, 0xFF}, {0x09, 0xCA}, {0x0A, 0x85},
      {0x0B, 0x00}, {0x0E, 0xBF}, {0x0F, 0x80}, {0x14, 0x0C}, {0x15, 0x0C}, {0x17, 0x02},
      {0x18, 0x26}, {0x19, 0x77}, {0x1A, 0xF4}, {0x1B, 0x66}, {0x1C, 0x44}, {0x1E, 0x00},
      {0x1F, 0x0C}, {0x20, 0x1A}, {0x21, 0x1A}, {0x00, 0x80}, {0x01, 0x3A}, {0x16, 0x3F},
      {0x16, 0x00}, {0xF9, 0x00}, {0x04, 0x01}, {0x17, 0x01}, {0x20, 0x10}, {0x21, 0x10},
      {0x00, 0x80}, {0x01, 0x3A}, {0x16, 0x3F}, {0x16, 0x00},
  };

  for (const auto &item : sequence) {
    if (!writeRegister(kEs7243eAddress, item[0], item[1])) {
      return false;
    }
  }
  return true;
}

uint8_t ESP32S3BoxLiteAudio::micGainRegister(float db) const {
  db += 0.5f;
  if (db <= 33.0f) {
    return static_cast<uint8_t>(db / 3.0f);
  }
  if (db < 36.0f) {
    return 12;
  }
  if (db < 37.0f) {
    return 13;
  }
  return 14;
}

bool ESP32S3BoxLiteAudio::setSpeakerVolumePercent(uint8_t percent) {
  volumePercent_ = std::min<uint8_t>(percent, 100);
  const uint8_t regValue = static_cast<uint8_t>((static_cast<uint16_t>(volumePercent_) * 255U) / 100U);
  return writeRegister(kEs8156Address, 0x14, regValue);
}

bool ESP32S3BoxLiteAudio::setMicGainDb(uint8_t db) {
  micGainDb_ = db;
  const uint8_t reg = static_cast<uint8_t>(0x10 | micGainRegister(static_cast<float>(db)));
  return writeRegister(kEs7243eAddress, 0x20, reg) && writeRegister(kEs7243eAddress, 0x21, reg);
}

bool ESP32S3BoxLiteAudio::playBeep(int frequencyHz, int durationMs) {
  if (!initialized_ || frequencyHz <= 0 || durationMs <= 0) {
    return false;
  }

  constexpr size_t kChunkSamples = 256;
  int16_t buffer[kChunkSamples];
  const int totalSamples = static_cast<int>((sampleRate() * static_cast<uint32_t>(durationMs)) / 1000U);
  float phase = 0.0f;
  const float phaseStep = (2.0f * kPi * static_cast<float>(frequencyHz)) / static_cast<float>(sampleRate());

  int samplesLeft = totalSamples;
  while (samplesLeft > 0) {
    const int chunkSamples = std::min<int>(samplesLeft, static_cast<int>(kChunkSamples));
    for (int i = 0; i < chunkSamples; ++i) {
      const float position = static_cast<float>(totalSamples - samplesLeft + i) / static_cast<float>(totalSamples);
      float envelope = 1.0f;
      if (position < 0.08f) {
        envelope = position / 0.08f;
      } else if (position > 0.85f) {
        envelope = (1.0f - position) / 0.15f;
      }
      envelope = std::max(0.0f, envelope);
      buffer[i] = static_cast<int16_t>(std::sin(phase) * 12000.0f * envelope);
      phase += phaseStep;
      if (phase > 2.0f * kPi) {
        phase -= 2.0f * kPi;
      }
    }
    if (writeSpeakerSamples(buffer, static_cast<size_t>(chunkSamples)) != static_cast<size_t>(chunkSamples)) {
      return false;
    }
    samplesLeft -= chunkSamples;
  }

  memset(buffer, 0, sizeof(buffer));
  writeSpeakerSamples(buffer, kChunkSamples / 2);
  return true;
}

size_t ESP32S3BoxLiteAudio::readMicSamples(int16_t *buffer, size_t sampleCount) {
  if (!initialized_ || buffer == nullptr || sampleCount == 0) {
    return 0;
  }

  size_t bytesRead = 0;
  const size_t requestedBytes = sampleCount * sizeof(int16_t);
  const esp_err_t err = i2s_read(static_cast<i2s_port_t>(kI2sPort), buffer, requestedBytes, &bytesRead, portMAX_DELAY);
  if (err != ESP_OK) {
    return 0;
  }
  return bytesRead / sizeof(int16_t);
}

size_t ESP32S3BoxLiteAudio::writeSpeakerSamples(const int16_t *buffer, size_t sampleCount) {
  if (!initialized_ || buffer == nullptr || sampleCount == 0) {
    return 0;
  }

  size_t bytesWritten = 0;
  const size_t requestedBytes = sampleCount * sizeof(int16_t);
  const esp_err_t err = i2s_write(static_cast<i2s_port_t>(kI2sPort), buffer, requestedBytes, &bytesWritten, portMAX_DELAY);
  if (err != ESP_OK) {
    return 0;
  }
  return bytesWritten / sizeof(int16_t);
}

int ESP32S3BoxLiteAudio::samplesToLevelPercent(const int16_t *buffer, size_t sampleCount) const {
  if (buffer == nullptr || sampleCount == 0) {
    return 0;
  }

  int peak = 0;
  for (size_t i = 0; i < sampleCount; ++i) {
    const int value = buffer[i] < 0 ? -buffer[i] : buffer[i];
    if (value > peak) {
      peak = value;
    }
  }
  return std::min(100, (peak * 100) / 32767);
}

bool ESP32S3BoxLite::begin(bool withDisplay, bool withInput, bool withAudio) {
  bool ok = true;
  if (withDisplay) {
    ok = display_.begin() && ok;
  }
  if (withInput) {
    ok = input_.begin() && ok;
  }
  if (withAudio) {
    ok = audio_.begin() && ok;
  }
  return ok;
}

ESP32S3BoxLiteDisplay &ESP32S3BoxLite::display() {
  return display_;
}

ESP32S3BoxLiteInput &ESP32S3BoxLite::input() {
  return input_;
}

ESP32S3BoxLiteAudio &ESP32S3BoxLite::audio() {
  return audio_;
}
