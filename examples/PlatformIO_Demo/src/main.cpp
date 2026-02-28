#include <Arduino.h>
#include <ESP32S3BoxLite.h>

namespace {

ESP32S3BoxLite box;

bool micMeterEnabled = true;
bool loopbackEnabled = false;
bool audioReady = false;
bool uiDirty = true;
uint32_t lastUiRefreshMs = 0;
int micLevelPercent = 0;
ESP32S3BoxLiteButton lastButton = ESP32S3BoxLiteButton::None;
size_t toneIndex = 0;
int16_t micBuffer[256];

constexpr int kToneFrequencies[] = {440, 660, 880};
constexpr int kUiRefreshMs = 120;

void drawStaticUi() {
  auto &display = box.display();
  display.fillScreen(ESP32S3BoxLiteDisplay::ColorBlue);
  display.fillRect(12, 12, ESP32S3BoxLiteDisplay::Width - 24, ESP32S3BoxLiteDisplay::Height - 24,
                   ESP32S3BoxLiteDisplay::ColorBlack);
  display.fillRect(24, 24, ESP32S3BoxLiteDisplay::Width - 48, 34, ESP32S3BoxLiteDisplay::ColorCyan);
  display.drawTextCentered(32, "AUDIO DEMO", 3, ESP32S3BoxLiteDisplay::ColorBlack,
                           ESP32S3BoxLiteDisplay::ColorCyan);
  display.drawText(24, 76, "CFG BEEP", 2, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
  display.drawText(24, 98, "ENT METER", 2, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
  display.drawText(24, 120, "NXT LOOP", 2, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
  display.drawText(24, 142, "PRV TONE", 2, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
  display.fillRect(24, 168, ESP32S3BoxLiteDisplay::Width - 48, 2, ESP32S3BoxLiteDisplay::ColorGray);
}

void drawDynamicUi(bool force = false) {
  const uint32_t now = millis();
  if (!force && !uiDirty && (now - lastUiRefreshMs) < kUiRefreshMs) {
    return;
  }

  char line[24];
  auto &display = box.display();

  display.fillRect(24, 176, ESP32S3BoxLiteDisplay::Width - 48, 48, ESP32S3BoxLiteDisplay::ColorBlack);
  snprintf(line, sizeof(line), "BTN %s", ESP32S3BoxLiteInput::buttonName(lastButton));
  display.drawText(24, 178, line, 2, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(line, sizeof(line), "MIC %s", micMeterEnabled ? "ON" : "OFF");
  display.drawText(24, 200, line, 2,
                   micMeterEnabled ? ESP32S3BoxLiteDisplay::ColorGreen : ESP32S3BoxLiteDisplay::ColorRed,
                   ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(line, sizeof(line), "LOP %s", loopbackEnabled ? "ON" : "OFF");
  display.drawText(170, 200, line, 2,
                   loopbackEnabled ? ESP32S3BoxLiteDisplay::ColorGreen : ESP32S3BoxLiteDisplay::ColorRed,
                   ESP32S3BoxLiteDisplay::ColorBlack);

  display.fillRect(24, 222, 272, 8, ESP32S3BoxLiteDisplay::ColorGray);
  display.fillRect(24, 222, (272 * micLevelPercent) / 100, 8, ESP32S3BoxLiteDisplay::ColorCyan);

  display.fillRect(24, 232, ESP32S3BoxLiteDisplay::Width - 48, 8, ESP32S3BoxLiteDisplay::ColorBlack);
  snprintf(line, sizeof(line), "HZ %d LV %03d", kToneFrequencies[toneIndex], micLevelPercent);
  display.drawText(24, 232, line, 1,
                   audioReady ? ESP32S3BoxLiteDisplay::ColorWhite : ESP32S3BoxLiteDisplay::ColorRed,
                   ESP32S3BoxLiteDisplay::ColorBlack);

  uiDirty = false;
  lastUiRefreshMs = now;
}

void handleButton(ESP32S3BoxLiteButton button) {
  lastButton = button;
  uiDirty = true;

  switch (button) {
    case ESP32S3BoxLiteButton::Config:
      if (audioReady) {
        box.audio().playBeep(kToneFrequencies[toneIndex], 120);
      }
      break;
    case ESP32S3BoxLiteButton::Prev:
      toneIndex = (toneIndex + 1) % (sizeof(kToneFrequencies) / sizeof(kToneFrequencies[0]));
      break;
    case ESP32S3BoxLiteButton::Enter:
      micMeterEnabled = !micMeterEnabled;
      if (!micMeterEnabled && !loopbackEnabled) {
        micLevelPercent = 0;
      }
      break;
    case ESP32S3BoxLiteButton::Next:
      loopbackEnabled = !loopbackEnabled;
      break;
    case ESP32S3BoxLiteButton::None:
    default:
      break;
  }
}

void updateAudio() {
  if (!audioReady || (!micMeterEnabled && !loopbackEnabled)) {
    if (micLevelPercent != 0) {
      micLevelPercent = 0;
      uiDirty = true;
    }
    return;
  }

  const size_t count = box.audio().readMicSamples(micBuffer, 256);
  if (count == 0) {
    return;
  }

  const int level = box.audio().samplesToLevelPercent(micBuffer, count);
  if (level != micLevelPercent) {
    micLevelPercent = level;
    uiDirty = true;
  }

  if (loopbackEnabled) {
    box.audio().writeSpeakerSamples(micBuffer, count);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  box.begin(true, true, false);
  drawStaticUi();

  audioReady = box.audio().begin();
  if (audioReady) {
    box.audio().playBeep(kToneFrequencies[toneIndex], 120);
  }

  drawDynamicUi(true);
}

void loop() {
  const ESP32S3BoxLiteButton button = box.input().pollButtonEvent();
  if (button != ESP32S3BoxLiteButton::None) {
    handleButton(button);
  }

  updateAudio();
  drawDynamicUi();
}
