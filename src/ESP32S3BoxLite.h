#pragma once

#include <Arduino.h>
#include <SPI.h>

#include <cstddef>
#include <cstdint>

enum class ESP32S3BoxLiteButton {
  None,
  Config,
  Prev,
  Enter,
  Next,
};

class ESP32S3BoxLiteDisplay {
 public:
  static constexpr uint16_t ColorBlack = 0x0000;
  static constexpr uint16_t ColorWhite = 0xFFFF;
  static constexpr uint16_t ColorRed = 0xF800;
  static constexpr uint16_t ColorGreen = 0x07E0;
  static constexpr uint16_t ColorBlue = 0x001F;
  static constexpr uint16_t ColorCyan = 0x07FF;
  static constexpr uint16_t ColorYellow = 0xFFE0;
  static constexpr uint16_t ColorGray = 0x8410;

  static constexpr uint16_t Width = 320;
  static constexpr uint16_t Height = 240;

  bool begin();
  void fillScreen(uint16_t color);
  void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
  void drawText(int16_t x, int16_t y, const char *text, uint8_t scale, uint16_t fg, uint16_t bg);
  void drawTextCentered(int16_t y, const char *text, uint8_t scale, uint16_t fg, uint16_t bg);

 private:
  void writeCommand(uint8_t command);
  void writeData(const uint8_t *data, size_t length);
  void writeCommandWithData(uint8_t command, const uint8_t *data, size_t length);
  void setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
  void sendColor(uint16_t color, uint32_t count);
  void drawGlyph(int16_t x, int16_t y, char ch, uint8_t scale, uint16_t fg, uint16_t bg);

  SPIClass spi_{FSPI};
  bool initialized_ = false;
};

class ESP32S3BoxLiteInput {
 public:
  bool begin();
  ESP32S3BoxLiteButton readRawButton();
  ESP32S3BoxLiteButton pollButtonEvent();
  int lastAdcMilliVolts() const;
  static const char *buttonName(ESP32S3BoxLiteButton button);

 private:
  uint32_t debounceStartedMs_ = 0;
  ESP32S3BoxLiteButton lastSample_ = ESP32S3BoxLiteButton::None;
  ESP32S3BoxLiteButton stableButton_ = ESP32S3BoxLiteButton::None;
  int lastAdcMilliVolts_ = 0;
};

class ESP32S3BoxLiteAudio {
 public:
  bool begin();
  bool ready() const;
  uint32_t sampleRate() const;
  bool setSpeakerVolumePercent(uint8_t percent);
  bool setMicGainDb(uint8_t db);
  bool playBeep(int frequencyHz, int durationMs = 180);
  size_t readMicSamples(int16_t *buffer, size_t sampleCount);
  size_t writeSpeakerSamples(const int16_t *buffer, size_t sampleCount);
  int samplesToLevelPercent(const int16_t *buffer, size_t sampleCount) const;

 private:
  bool initI2cBus();
  bool initI2sBus();
  bool writeRegister(uint8_t deviceAddress, uint8_t reg, uint8_t value);
  bool readRegister(uint8_t deviceAddress, uint8_t reg, uint8_t &value);
  bool initEs8156();
  bool startEs8156();
  bool initEs7243e();
  uint8_t micGainRegister(float db) const;

  bool initialized_ = false;
  uint8_t volumePercent_ = 65;
  uint8_t micGainDb_ = 24;
};

class ESP32S3BoxLite {
 public:
  bool begin(bool withDisplay = true, bool withInput = true, bool withAudio = true);
  ESP32S3BoxLiteDisplay &display();
  ESP32S3BoxLiteInput &input();
  ESP32S3BoxLiteAudio &audio();

 private:
  ESP32S3BoxLiteDisplay display_;
  ESP32S3BoxLiteInput input_;
  ESP32S3BoxLiteAudio audio_;
};
