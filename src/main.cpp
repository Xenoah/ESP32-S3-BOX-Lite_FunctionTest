#include <Arduino.h>
#include <SPI.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "driver/i2c.h"
#include "driver/i2s.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "es7243e_adc.h"
#include "es8156_dac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

constexpr uint16_t kScreenWidth = 320;
constexpr uint16_t kScreenHeight = 240;

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_WHITE = 0xFFFF;
constexpr uint16_t COLOR_RED = 0xF800;
constexpr uint16_t COLOR_GREEN = 0x07E0;
constexpr uint16_t COLOR_BLUE = 0x001F;
constexpr uint16_t COLOR_CYAN = 0x07FF;
constexpr uint16_t COLOR_YELLOW = 0xFFE0;
constexpr uint16_t COLOR_GRAY = 0x8410;

constexpr uint32_t kSampleRate = 22050;
constexpr size_t kAudioBufferSamples = 256;
constexpr size_t kAudioBufferBytes = kAudioBufferSamples * sizeof(int16_t);
constexpr int kMicGainDb = 24;
constexpr int kSpeakerVolume = 65;
constexpr int kUiRefreshMs = 120;
constexpr int kDebounceMs = 25;

constexpr int kButtonNextMinMv = 720;
constexpr int kButtonNextMaxMv = 920;
constexpr int kButtonEnterMinMv = 1880;
constexpr int kButtonEnterMaxMv = 2080;
constexpr int kButtonPrevMinMv = 2310;
constexpr int kButtonPrevMaxMv = 2510;

constexpr int kToneFreqs[] = {440, 660, 880};

constexpr float kPi = 3.14159265358979323846f;

SPIClass lcdSpi(FSPI);

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

enum class ButtonId {
  None,
  Config,
  Prev,
  Enter,
  Next,
};

struct AudioState {
  bool ready = false;
  const audio_codec_data_if_t *dataIf = nullptr;
  const audio_codec_ctrl_if_t *speakerCtrlIf = nullptr;
  const audio_codec_ctrl_if_t *micCtrlIf = nullptr;
  const audio_codec_gpio_if_t *gpioIf = nullptr;
  const audio_codec_if_t *speakerCodecIf = nullptr;
  const audio_codec_if_t *micCodecIf = nullptr;
  esp_codec_dev_handle_t speaker = nullptr;
  esp_codec_dev_handle_t mic = nullptr;
};

AudioState gAudio;

bool gMicMeterEnabled = true;
bool gLoopbackEnabled = false;
bool gUiDirty = true;
bool gAudioError = false;
uint32_t gLastUiRefreshMs = 0;
uint32_t gDebounceStartedMs = 0;
ButtonId gLastButtonSample = ButtonId::None;
ButtonId gStableButton = ButtonId::None;
ButtonId gLastTriggeredButton = ButtonId::None;
int gMicLevelPercent = 0;
size_t gToneIndex = 0;
int16_t gAudioBuffer[kAudioBufferSamples];

const Glyph *findGlyph(char ch) {
  for (const auto &glyph : kGlyphs) {
    if (glyph.ch == ch) {
      return &glyph;
    }
  }
  return &kGlyphs[0];
}

const char *buttonLabel(ButtonId button) {
  switch (button) {
    case ButtonId::Config:
      return "CFG";
    case ButtonId::Prev:
      return "PRV";
    case ButtonId::Enter:
      return "ENT";
    case ButtonId::Next:
      return "NXT";
    case ButtonId::None:
    default:
      return "NONE";
  }
}

void writeCommand(uint8_t command) {
  digitalWrite(kLcdDcPin, LOW);
  digitalWrite(kLcdCsPin, LOW);
  lcdSpi.write(command);
  digitalWrite(kLcdCsPin, HIGH);
}

void writeData(const uint8_t *data, size_t length) {
  digitalWrite(kLcdDcPin, HIGH);
  digitalWrite(kLcdCsPin, LOW);
  lcdSpi.writeBytes(data, length);
  digitalWrite(kLcdCsPin, HIGH);
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

  digitalWrite(kLcdDcPin, HIGH);
  digitalWrite(kLcdCsPin, LOW);
  while (count > 0) {
    const uint32_t chunkPixels = std::min<uint32_t>(count, sizeof(buffer) / 2);
    lcdSpi.writeBytes(buffer, chunkPixels * 2);
    count -= chunkPixels;
  }
  digitalWrite(kLcdCsPin, HIGH);
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

void drawText(int16_t x, int16_t y, const char *text, uint8_t scale, uint16_t fg, uint16_t bg) {
  const int16_t charWidth = 6 * scale;
  for (size_t i = 0; text[i] != '\0'; ++i) {
    drawGlyph(x, y, text[i], scale, fg, bg);
    x += charWidth;
  }
}

void drawTextCentered(int16_t y, const char *text, uint8_t scale, uint16_t fg, uint16_t bg) {
  const size_t len = strlen(text);
  const int16_t charWidth = 6 * scale;
  const int16_t textWidth = static_cast<int16_t>(len) * charWidth - scale;
  const int16_t x = (kScreenWidth - textWidth) / 2;
  drawText(x, y, text, scale, fg, bg);
}

void initDisplay() {
  pinMode(kLcdCsPin, OUTPUT);
  pinMode(kLcdDcPin, OUTPUT);
  pinMode(kLcdRstPin, OUTPUT);
  pinMode(kLcdBacklightPin, OUTPUT);

  digitalWrite(kLcdCsPin, HIGH);
  digitalWrite(kLcdDcPin, HIGH);
  digitalWrite(kLcdBacklightPin, !kBacklightOnLevel);

  lcdSpi.begin(kLcdClkPin, -1, kLcdMosiPin, kLcdCsPin);
  lcdSpi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));

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
}

void drawStaticUi() {
  fillScreen(COLOR_BLUE);
  fillRect(12, 12, kScreenWidth - 24, kScreenHeight - 24, COLOR_BLACK);
  fillRect(24, 24, kScreenWidth - 48, 34, COLOR_CYAN);
  drawTextCentered(32, "AUDIO DEMO", 3, COLOR_BLACK, COLOR_CYAN);

  drawText(24, 76, "CFG BEEP", 2, COLOR_WHITE, COLOR_BLACK);
  drawText(24, 98, "ENT METER", 2, COLOR_WHITE, COLOR_BLACK);
  drawText(24, 120, "NXT LOOP", 2, COLOR_WHITE, COLOR_BLACK);
  drawText(24, 142, "PRV TONE", 2, COLOR_WHITE, COLOR_BLACK);
  fillRect(24, 168, kScreenWidth - 48, 2, COLOR_GRAY);
}

void drawDynamicUi() {
  char line[24];

  fillRect(24, 176, kScreenWidth - 48, 48, COLOR_BLACK);
  snprintf(line, sizeof(line), "BTN %s", buttonLabel(gLastTriggeredButton));
  drawText(24, 178, line, 2, COLOR_YELLOW, COLOR_BLACK);

  snprintf(line, sizeof(line), "MIC %s", gMicMeterEnabled ? "ON" : "OFF");
  drawText(24, 200, line, 2, gMicMeterEnabled ? COLOR_GREEN : COLOR_RED, COLOR_BLACK);

  snprintf(line, sizeof(line), "LOP %s", gLoopbackEnabled ? "ON" : "OFF");
  drawText(170, 200, line, 2, gLoopbackEnabled ? COLOR_GREEN : COLOR_RED, COLOR_BLACK);

  fillRect(24, 222, 272, 8, COLOR_GRAY);
  fillRect(24, 222, (272 * gMicLevelPercent) / 100, 8, COLOR_CYAN);

  fillRect(24, 232, kScreenWidth - 48, 12, COLOR_BLACK);
  snprintf(line, sizeof(line), "HZ %d  LV %03d", kToneFreqs[gToneIndex], gMicLevelPercent);
  drawText(24, 232, line, 1, gAudioError ? COLOR_RED : COLOR_WHITE, COLOR_BLACK);
}

void refreshUi(bool force = false) {
  const uint32_t now = millis();
  if (!force && !gUiDirty && (now - gLastUiRefreshMs) < kUiRefreshMs) {
    return;
  }
  drawDynamicUi();
  gLastUiRefreshMs = now;
  gUiDirty = false;
}

bool initI2cBus() {
  i2c_config_t config = {};
  config.mode = I2C_MODE_MASTER;
  config.sda_io_num = static_cast<gpio_num_t>(kI2cSdaPin);
  config.scl_io_num = static_cast<gpio_num_t>(kI2cSclPin);
  config.sda_pullup_en = GPIO_PULLUP_ENABLE;
  config.scl_pullup_en = GPIO_PULLUP_ENABLE;
  config.master.clk_speed = 100000;

  esp_err_t err = i2c_param_config(static_cast<i2c_port_t>(kI2cPort), &config);
  if (err != ESP_OK) {
    Serial.printf("i2c_param_config failed: %d\n", err);
    return false;
  }
  err = i2c_driver_install(static_cast<i2c_port_t>(kI2cPort), config.mode, 0, 0, 0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("i2c_driver_install failed: %d\n", err);
    return false;
  }
  return true;
}

bool initI2sBus() {
  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  config.sample_rate = kSampleRate;
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
    Serial.printf("i2s_driver_install failed: %d\n", err);
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
    Serial.printf("i2s_set_pin failed: %d\n", err);
    return false;
  }
  return true;
}

bool initAudio() {
  if (!initI2cBus() || !initI2sBus()) {
    return false;
  }

  audio_codec_i2s_cfg_t i2sCfg = {};
  i2sCfg.port = kI2sPort;
  gAudio.dataIf = audio_codec_new_i2s_data(&i2sCfg);

  audio_codec_i2c_cfg_t speakerI2cCfg = {};
  speakerI2cCfg.port = kI2cPort;
  speakerI2cCfg.addr = ES8156_CODEC_DEFAULT_ADDR;
  gAudio.speakerCtrlIf = audio_codec_new_i2c_ctrl(&speakerI2cCfg);

  audio_codec_i2c_cfg_t micI2cCfg = {};
  micI2cCfg.port = kI2cPort;
  micI2cCfg.addr = ES7243E_CODEC_DEFAULT_ADDR;
  gAudio.micCtrlIf = audio_codec_new_i2c_ctrl(&micI2cCfg);

  gAudio.gpioIf = audio_codec_new_gpio();
  if (!gAudio.dataIf || !gAudio.speakerCtrlIf || !gAudio.micCtrlIf || !gAudio.gpioIf) {
    Serial.println("audio interface allocation failed");
    return false;
  }

  es8156_codec_cfg_t speakerCodecCfg = {};
  speakerCodecCfg.ctrl_if = gAudio.speakerCtrlIf;
  speakerCodecCfg.gpio_if = gAudio.gpioIf;
  speakerCodecCfg.pa_pin = kPowerAmpPin;
  speakerCodecCfg.pa_reverted = false;
  speakerCodecCfg.hw_gain.pa_voltage = 5.0f;
  speakerCodecCfg.hw_gain.codec_dac_voltage = 3.3f;
  speakerCodecCfg.hw_gain.pa_gain = 0.0f;
  gAudio.speakerCodecIf = es8156_codec_new(&speakerCodecCfg);

  es7243e_codec_cfg_t micCodecCfg = {};
  micCodecCfg.ctrl_if = gAudio.micCtrlIf;
  gAudio.micCodecIf = es7243e_codec_new(&micCodecCfg);

  if (!gAudio.speakerCodecIf || !gAudio.micCodecIf) {
    Serial.println("codec allocation failed");
    return false;
  }

  esp_codec_dev_cfg_t speakerDevCfg = {};
  speakerDevCfg.dev_type = ESP_CODEC_DEV_TYPE_OUT;
  speakerDevCfg.codec_if = gAudio.speakerCodecIf;
  speakerDevCfg.data_if = gAudio.dataIf;
  gAudio.speaker = esp_codec_dev_new(&speakerDevCfg);

  esp_codec_dev_cfg_t micDevCfg = {};
  micDevCfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
  micDevCfg.codec_if = gAudio.micCodecIf;
  micDevCfg.data_if = gAudio.dataIf;
  gAudio.mic = esp_codec_dev_new(&micDevCfg);

  if (!gAudio.speaker || !gAudio.mic) {
    Serial.println("codec device creation failed");
    return false;
  }

  esp_codec_dev_sample_info_t sampleInfo = {};
  sampleInfo.sample_rate = kSampleRate;
  sampleInfo.channel = 1;
  sampleInfo.bits_per_sample = 16;
  sampleInfo.channel_mask = 0;
  sampleInfo.mclk_multiple = 0;

  int err = esp_codec_dev_open(gAudio.speaker, &sampleInfo);
  if (err != ESP_CODEC_DEV_OK) {
    Serial.printf("speaker open failed: %d\n", err);
    return false;
  }
  err = esp_codec_dev_open(gAudio.mic, &sampleInfo);
  if (err != ESP_CODEC_DEV_OK) {
    Serial.printf("mic open failed: %d\n", err);
    return false;
  }

  err = esp_codec_dev_set_out_vol(gAudio.speaker, kSpeakerVolume);
  if (err != ESP_CODEC_DEV_OK) {
    Serial.printf("speaker volume failed: %d\n", err);
    return false;
  }
  err = esp_codec_dev_set_in_gain(gAudio.mic, static_cast<float>(kMicGainDb));
  if (err != ESP_CODEC_DEV_OK) {
    Serial.printf("mic gain failed: %d\n", err);
    return false;
  }

  gAudio.ready = true;
  return true;
}

void playBeep(int frequencyHz, int durationMs = 180) {
  if (!gAudio.ready) {
    return;
  }

  const int totalSamples = (kSampleRate * durationMs) / 1000;
  float phase = 0.0f;
  const float phaseStep = (2.0f * kPi * static_cast<float>(frequencyHz)) / static_cast<float>(kSampleRate);

  int samplesLeft = totalSamples;
  while (samplesLeft > 0) {
    const int chunkSamples = std::min<int>(samplesLeft, static_cast<int>(kAudioBufferSamples));
    for (int i = 0; i < chunkSamples; ++i) {
      const float position = static_cast<float>(totalSamples - samplesLeft + i) / static_cast<float>(totalSamples);
      float envelope = 1.0f;
      if (position < 0.08f) {
        envelope = position / 0.08f;
      } else if (position > 0.85f) {
        envelope = (1.0f - position) / 0.15f;
      }
      envelope = std::max(0.0f, envelope);
      gAudioBuffer[i] = static_cast<int16_t>(std::sin(phase) * 12000.0f * envelope);
      phase += phaseStep;
      if (phase > 2.0f * kPi) {
        phase -= 2.0f * kPi;
      }
    }
    esp_codec_dev_write(gAudio.speaker, gAudioBuffer, chunkSamples * sizeof(int16_t));
    samplesLeft -= chunkSamples;
  }

  memset(gAudioBuffer, 0, kAudioBufferBytes);
  esp_codec_dev_write(gAudio.speaker, gAudioBuffer, kAudioBufferBytes / 2);
}

ButtonId readAdcButtons() {
  const int mv = static_cast<int>(analogReadMilliVolts(kAdcButtonPin));
  if (mv >= kButtonPrevMinMv && mv <= kButtonPrevMaxMv) {
    return ButtonId::Prev;
  }
  if (mv >= kButtonEnterMinMv && mv <= kButtonEnterMaxMv) {
    return ButtonId::Enter;
  }
  if (mv >= kButtonNextMinMv && mv <= kButtonNextMaxMv) {
    return ButtonId::Next;
  }
  return ButtonId::None;
}

ButtonId readPhysicalButton() {
  if (digitalRead(kConfigButtonPin) == LOW) {
    return ButtonId::Config;
  }
  return readAdcButtons();
}

ButtonId pollButtonEvent() {
  const uint32_t now = millis();
  const ButtonId sample = readPhysicalButton();

  if (sample != gLastButtonSample) {
    gLastButtonSample = sample;
    gDebounceStartedMs = now;
  }

  if (sample != gStableButton && (now - gDebounceStartedMs) >= kDebounceMs) {
    gStableButton = sample;
    if (gStableButton != ButtonId::None) {
      return gStableButton;
    }
  }

  return ButtonId::None;
}

void handleButton(ButtonId button) {
  gLastTriggeredButton = button;
  gUiDirty = true;

  switch (button) {
    case ButtonId::Config:
      Serial.printf("Button CFG -> beep %d Hz\n", kToneFreqs[gToneIndex]);
      playBeep(kToneFreqs[gToneIndex]);
      break;
    case ButtonId::Prev:
      gToneIndex = (gToneIndex + 1) % (sizeof(kToneFreqs) / sizeof(kToneFreqs[0]));
      Serial.printf("Button PRV -> tone %d Hz\n", kToneFreqs[gToneIndex]);
      break;
    case ButtonId::Enter:
      gMicMeterEnabled = !gMicMeterEnabled;
      if (!gMicMeterEnabled && !gLoopbackEnabled) {
        gMicLevelPercent = 0;
      }
      Serial.printf("Button ENT -> mic meter %s\n", gMicMeterEnabled ? "ON" : "OFF");
      break;
    case ButtonId::Next:
      gLoopbackEnabled = !gLoopbackEnabled;
      Serial.printf("Button NXT -> loopback %s\n", gLoopbackEnabled ? "ON" : "OFF");
      break;
    case ButtonId::None:
    default:
      break;
  }
}

void updateMicAudio() {
  if (!gAudio.ready) {
    return;
  }
  if (!gMicMeterEnabled && !gLoopbackEnabled) {
    if (gMicLevelPercent != 0) {
      gMicLevelPercent = 0;
      gUiDirty = true;
    }
    return;
  }

  const int err = esp_codec_dev_read(gAudio.mic, gAudioBuffer, sizeof(gAudioBuffer));
  if (err != ESP_CODEC_DEV_OK) {
    Serial.printf("mic read failed: %d\n", err);
    gAudioError = true;
    gUiDirty = true;
    return;
  }

  int peak = 0;
  for (const int16_t sample : gAudioBuffer) {
    const int value = sample < 0 ? -sample : sample;
    if (value > peak) {
      peak = value;
    }
  }

  const int level = std::min(100, (peak * 100) / 32767);
  if (level != gMicLevelPercent) {
    gMicLevelPercent = level;
    gUiDirty = true;
  }

  if (gLoopbackEnabled) {
    const int writeErr = esp_codec_dev_write(gAudio.speaker, gAudioBuffer, sizeof(gAudioBuffer));
    if (writeErr != ESP_CODEC_DEV_OK) {
      Serial.printf("speaker write failed: %d\n", writeErr);
      gAudioError = true;
      gUiDirty = true;
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("BOX-Lite I/O demo start");

  pinMode(kConfigButtonPin, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(kAdcButtonPin, ADC_11db);

  initDisplay();
  drawStaticUi();

  gAudio.ready = initAudio();
  gAudioError = !gAudio.ready;
  if (gAudio.ready) {
    Serial.println("Audio init OK");
    playBeep(kToneFreqs[gToneIndex], 120);
  } else {
    Serial.println("Audio init FAILED");
  }

  gUiDirty = true;
  refreshUi(true);
}

void loop() {
  const ButtonId button = pollButtonEvent();
  if (button != ButtonId::None) {
    handleButton(button);
  }

  updateMicAudio();
  refreshUi();
}

extern "C" void app_main(void);

extern void initArduino();
extern void serialEventRun(void) __attribute__((weak));

extern "C" void app_main(void) {
  initArduino();
  setup();

  while (true) {
    loop();
    if (serialEventRun) {
      serialEventRun();
    }
    vTaskDelay(1);
  }
}
