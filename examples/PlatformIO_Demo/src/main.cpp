// PlatformIO_Demo - main.cpp
// Demonstrates all features of the ESP32S3BoxLite library (Phases 1-7).
// Identical in functionality to FullFunctionDemo.ino, structured for PlatformIO.

#include <Arduino.h>
#include <ESP32S3BoxLite.h>

namespace {

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

ESP32S3BoxLite box;

enum class DemoPage {
  Boot,
  Display,
  DrawPrimitives,
  Sprite,
  Input,
  Audio,
  System,
  kCount
};

DemoPage currentPage  = DemoPage::Boot;
bool     pageChanged  = true;
bool     audioReady   = false;
uint32_t lastLoopMs   = 0;

uint8_t  volumePercent = 65;
int      toneFreqHz    = 440;
int      micLevel      = 0;

char sysLine[64];

ESP32S3BoxLiteSprite sprite;
bool spriteCreated = false;
int  spriteAngle   = 0;

static constexpr float kPiLocal = 3.14159265f;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

void drawPage();
void handleButton(ESP32S3BoxLiteButton btn, ButtonEvent evt);
void drawBootPage();
void drawDisplayPage();
void drawPrimitivesPage();
void drawSpritePage();
void drawInputPage();
void drawAudioPage();
void drawSystemPage();
void nextPage();

// ---------------------------------------------------------------------------
// Button callback
// ---------------------------------------------------------------------------

void onButton(ESP32S3BoxLiteButton btn, ButtonEvent evt) {
  handleButton(btn, evt);
}

void handleButton(ESP32S3BoxLiteButton btn, ButtonEvent evt) {
  if (evt == ButtonEvent::Pressed) {
    if (btn == ESP32S3BoxLiteButton::Config) {
      nextPage();
      if (audioReady) { box.audio().playBeep(880, 80); }
    } else if (btn == ESP32S3BoxLiteButton::Next) {
      if (currentPage == DemoPage::Audio) {
        volumePercent = (volumePercent < 100) ? volumePercent + 10 : 100;
        if (audioReady) { box.audio().setSpeakerVolumePercent(volumePercent); }
      }
      pageChanged = true;
    } else if (btn == ESP32S3BoxLiteButton::Prev) {
      if (currentPage == DemoPage::Audio) {
        volumePercent = (volumePercent > 10) ? volumePercent - 10 : 10;
        if (audioReady) { box.audio().setSpeakerVolumePercent(volumePercent); }
      }
      pageChanged = true;
    } else if (btn == ESP32S3BoxLiteButton::Enter) {
      if (currentPage == DemoPage::Audio && audioReady) {
        toneFreqHz = (toneFreqHz == 440) ? 880 : 440;
        box.audio().playBeep(toneFreqHz, 120);
      } else if (currentPage == DemoPage::System) {
        box.display().showMessage("Hello from showMessage!", ESP32S3BoxLiteDisplay::ColorBlue);
        delay(1500);
        pageChanged = true;
      }
      pageChanged = true;
    }
  }

  if (evt == ButtonEvent::LongPressed) {
    if (btn == ESP32S3BoxLiteButton::Config && audioReady) {
      box.audio().beepPattern(BeepPattern::Error);
    }
  }
}

void nextPage() {
  int p = static_cast<int>(currentPage) + 1;
  if (p >= static_cast<int>(DemoPage::kCount)) { p = 1; }
  currentPage = static_cast<DemoPage>(p);
  pageChanged = true;
}

// ---------------------------------------------------------------------------
// Page drawing functions
// ---------------------------------------------------------------------------

void drawPage() {
  switch (currentPage) {
    case DemoPage::Boot:           drawBootPage();        break;
    case DemoPage::Display:        drawDisplayPage();     break;
    case DemoPage::DrawPrimitives: drawPrimitivesPage();  break;
    case DemoPage::Sprite:         drawSpritePage();      break;
    case DemoPage::Input:          drawInputPage();       break;
    case DemoPage::Audio:          drawAudioPage();       break;
    case DemoPage::System:         drawSystemPage();      break;
    default: break;
  }
}

void drawBootPage() {
  auto &d = box.display();
  d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawStatusBar("ESP32S3BOX-Lite", "DEMO", ESP32S3BoxLiteDisplay::ColorBlue);

  d.drawTextCentered(30, "FULL FUNCTION", 2, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawTextCentered(52, "DEMO v2.0", 2, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  d.drawText(10, 90,  "CFG = next page", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 104, "NXT/PRV = adjust", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 118, "ENT = select / long-press", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);

  d.drawText(10, 140, "Pages:", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 152, "Display  Primitives  Sprite", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 164, "Input  Audio  System", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Heap: %lu B  CPU: %lu MHz",
           (unsigned long)box.getFreeHeap(), (unsigned long)box.getCpuFreqMHz());
  d.drawText(10, 200, sysLine, 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Reset: %s  Wakeup: %s", box.getResetReason(), box.getWakeupReason());
  d.drawText(10, 214, sysLine, 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Audio: %s", audioReady ? "OK" : "FAIL");
  d.drawText(10, 228, sysLine, 1,
             audioReady ? ESP32S3BoxLiteDisplay::ColorGreen : ESP32S3BoxLiteDisplay::ColorRed,
             ESP32S3BoxLiteDisplay::ColorBlack);
}

void drawDisplayPage() {
  auto &d = box.display();
  d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawStatusBar("DISPLAY", "Phase 1", ESP32S3BoxLiteDisplay::ColorBlue);

  d.setBacklight(80);

  d.drawText(10, 24, "Uppercase ABCDEFGHIJKLMNOPQRST", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 36, "Lowercase abcdefghijklmnopqrst", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 48, "Digits 0123456789 Symbols !?:.", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);

  d.printf(10, 62, 1, ESP32S3BoxLiteDisplay::ColorGreen, ESP32S3BoxLiteDisplay::ColorBlack,
           "printf: heap=%lu sr=%lu", (unsigned long)box.getFreeHeap(), (unsigned long)box.audio().sampleRate());

  d.drawText(10, 80, "Progress bars (25/50/75/100%):", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawProgressBar(10,  92, 300, 10, 25,  ESP32S3BoxLiteDisplay::ColorRed,    ESP32S3BoxLiteDisplay::ColorGray);
  d.drawProgressBar(10, 106, 300, 10, 50,  ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorGray);
  d.drawProgressBar(10, 120, 300, 10, 75,  ESP32S3BoxLiteDisplay::ColorGreen,  ESP32S3BoxLiteDisplay::ColorGray);
  d.drawProgressBar(10, 134, 300, 10, 100, ESP32S3BoxLiteDisplay::ColorCyan,   ESP32S3BoxLiteDisplay::ColorGray);

  d.drawText(10, 154, "Color swatches:", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  constexpr uint16_t colors[] = {
    ESP32S3BoxLiteDisplay::ColorRed,    ESP32S3BoxLiteDisplay::ColorGreen,
    ESP32S3BoxLiteDisplay::ColorBlue,   ESP32S3BoxLiteDisplay::ColorCyan,
    ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorOrange,
    ESP32S3BoxLiteDisplay::ColorPurple, ESP32S3BoxLiteDisplay::ColorWhite,
  };
  for (int i = 0; i < 8; ++i) {
    d.fillRect(static_cast<uint16_t>(10 + i * 38), 166, 34, 22, colors[i]);
  }

  d.drawText(10, 204, "showMessage / showError / showBootScreen", 1,
             ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 218, "drawStatusBar - see top of every page", 1,
             ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  d.setBacklight(100);
}

void drawPrimitivesPage() {
  auto &d = box.display();
  d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawStatusBar("PRIMITIVES", "Phase 1", ESP32S3BoxLiteDisplay::ColorBlue);

  // Pixel row
  for (int i = 0; i < 30; ++i) {
    d.drawPixel(10 + i * 2, 32, (i % 2 == 0) ? ESP32S3BoxLiteDisplay::ColorWhite : ESP32S3BoxLiteDisplay::ColorGray);
  }

  // Lines
  d.drawLine(10, 46, 310, 46, ESP32S3BoxLiteDisplay::ColorRed);
  d.drawLine(10, 56, 200, 90, ESP32S3BoxLiteDisplay::ColorGreen);
  d.drawLine(200, 56, 10, 90, ESP32S3BoxLiteDisplay::ColorBlue);

  // Rectangles
  d.drawRect(10,  102, 90, 40, ESP32S3BoxLiteDisplay::ColorCyan);
  d.drawRect(115, 102, 90, 40, ESP32S3BoxLiteDisplay::ColorYellow);
  d.drawRect(220, 102, 90, 40, ESP32S3BoxLiteDisplay::ColorOrange);
  d.fillRect(10,  102, 90, 40, ESP32S3BoxLiteDisplay::ColorCyan);     // filled version same area - demo overlap
  d.drawRect(10,  102, 90, 40, ESP32S3BoxLiteDisplay::ColorWhite);    // outline on top

  // Circles
  d.drawCircle( 45, 175, 30, ESP32S3BoxLiteDisplay::ColorRed);
  d.drawCircle(120, 175, 30, ESP32S3BoxLiteDisplay::ColorGreen);
  d.fillCircle(200, 175, 30, ESP32S3BoxLiteDisplay::ColorBlue);
  d.fillCircle(275, 175, 30, ESP32S3BoxLiteDisplay::ColorPurple);

  // 1-bit bitmap (checkerboard 8x8)
  static const uint8_t kCheck[] = {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
  d.drawBitmap(10, 218, kCheck, 8, 8, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorGray);

  // RGB565 bitmap 4x4
  static const uint16_t kRgb[] = {
    0xF800, 0x07E0, 0x001F, 0xFFE0,
    0xF810, 0x07F0, 0x041F, 0xFFF0,
    0xFC00, 0x0800, 0x780F, 0x8410,
    0xFFFF, 0x8410, 0xF800, 0x001F,
  };
  d.drawRGBBitmap(30, 218, kRgb, 4, 4);
  d.drawText(50, 220, "drawPixel drawLine drawRect", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(50, 230, "drawCircle fillCircle bitmaps", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
}

void drawSpritePage() {
  auto &d = box.display();

  if (!spriteCreated) {
    d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawStatusBar("SPRITE", "Phase 2", ESP32S3BoxLiteDisplay::ColorBlue);
    d.drawTextCentered(120, "Allocating sprite...", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

    spriteCreated = sprite.createSprite(200, 150);
    if (!spriteCreated) {
      d.showError("Sprite alloc failed");
      return;
    }
    d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawStatusBar("SPRITE", "Phase 2", ESP32S3BoxLiteDisplay::ColorBlue);
  }

  // Fill and draw content into sprite buffer
  sprite.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
  sprite.drawText(2, 2, "Sprite buffer", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);

  // Animated circle inside sprite
  const int16_t cx = 100;
  const int16_t cy = 75;
  const int16_t r  = 30 + static_cast<int16_t>(spriteAngle % 15) - 7;
  for (int i = 0; i < 360; i += 8) {
    const float rad = static_cast<float>(i + spriteAngle) * kPiLocal / 180.0f;
    const int16_t sx = static_cast<int16_t>(cx + r * cosf(rad));
    const int16_t sy = static_cast<int16_t>(cy + r * sinf(rad));
    const uint16_t color = (i < 120) ? ESP32S3BoxLiteDisplay::ColorRed :
                           (i < 240) ? ESP32S3BoxLiteDisplay::ColorGreen : ESP32S3BoxLiteDisplay::ColorBlue;
    sprite.drawPixel(sx, sy, color);
  }

  // Inner filled rect
  sprite.fillRect(80, 55, 40, 40, ESP32S3BoxLiteDisplay::ColorPurple);
  sprite.drawText(82, 65, "SP", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorPurple);

  // Footer bar
  sprite.fillRect(0, 134, 200, 16, ESP32S3BoxLiteDisplay::ColorBlue);
  sprite.drawText(4, 137, "No flicker - DMA push", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlue);

  // Push to display
  sprite.pushSprite(d, 60, 44);

  // Info below sprite
  d.fillRect(0, 198, ESP32S3BoxLiteDisplay::Width, 42, ESP32S3BoxLiteDisplay::ColorBlack);
  d.printf(10, 200, 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack,
           "Sprite %dx%d  angle=%d", sprite.width(), sprite.height(), spriteAngle);
  d.drawText(10, 214, "pushSprite bulk-transfers the buffer via SPI", 1,
             ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 228, "eliminating pixel-by-pixel flicker", 1,
             ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);

  spriteAngle = (spriteAngle + 4) % 360;
}

void drawInputPage() {
  auto &d = box.display();
  d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawStatusBar("INPUT", "Phase 3", ESP32S3BoxLiteDisplay::ColorBlue);

  d.drawText(10, 24, "Phase 3 input features:", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  d.drawText(10, 38,  "pollButtonEventEx() -> Pressed/LongPressed/Released", 1,
             ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 50, "setLongPressThresholdMs(600)", 1,
             ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 62, "isDoubleClick() -> 400ms window", 1,
             ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 74, "setButtonCallback() -> async events", 1,
             ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 86, "waitForButton(timeoutMs) -> blocking", 1,
             ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);

  d.fillRect(0, 100, ESP32S3BoxLiteDisplay::Width, 2, ESP32S3BoxLiteDisplay::ColorGray);

  d.drawText(10, 108, "Live button test (waitForButton 2s):", 1,
             ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 122, "Press any button...", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  ESP32S3BoxLiteButton pressed = box.input().waitForButton(2000);
  char msg[48];
  snprintf(msg, sizeof(msg), "Received: %s", ESP32S3BoxLiteInput::buttonName(pressed));
  d.fillRect(0, 136, ESP32S3BoxLiteDisplay::Width, 14, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 136, msg, 1,
             (pressed != ESP32S3BoxLiteButton::None) ? ESP32S3BoxLiteDisplay::ColorGreen : ESP32S3BoxLiteDisplay::ColorRed,
             ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(msg, sizeof(msg), "ADC: %d mV", box.input().lastAdcMilliVolts());
  d.drawText(10, 152, msg, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  d.drawText(10, 176, "Double-click ENT -> OK beep", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 190, "Long-press CFG -> Error beep", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 204, "Callback active for all pages", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
}

void drawAudioPage() {
  auto &d = box.display();

  static DemoPage lastPage = DemoPage::Boot;
  if (lastPage != DemoPage::Audio) {
    lastPage = DemoPage::Audio;
    d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawStatusBar("AUDIO", "Phase 4/5", ESP32S3BoxLiteDisplay::ColorBlue);
    d.drawText(10, 24, "NXT=vol+  PRV=vol-  ENT=beep", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 38, "Volume:", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 60, "Mic level:", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 82, "Beep patterns:", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 96, "OK  Error  Double  Triple", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 116, "Phase 5: SPIFFS record/play", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 130, "startMicRecord / stopMicRecord", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 144, "saveMicToSPIFFS / playWavFromSPIFFS", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 158, "playWav(ptr, len) - parse WAV header", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 172, "setVolumeFade(target, durationMs)", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 186, "setMute / muteToggle / setSampleRate", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
  }

  if (!audioReady) {
    d.fillRect(0, 200, ESP32S3BoxLiteDisplay::Width, 40, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 210, "Audio not initialized!", 1, ESP32S3BoxLiteDisplay::ColorRed, ESP32S3BoxLiteDisplay::ColorBlack);
    return;
  }

  // Volume bar (dynamic)
  d.drawProgressBar(10, 48, 280, 10, volumePercent, ESP32S3BoxLiteDisplay::ColorGreen, ESP32S3BoxLiteDisplay::ColorGray);
  char buf[32];
  snprintf(buf, sizeof(buf), "%3d%%", volumePercent);
  d.fillRect(294, 38, 26, 12, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(294, 38, buf, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  // Mic level bar (dynamic)
  d.drawProgressBar(10, 70, 280, 10, static_cast<uint8_t>(micLevel), ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorGray);
  snprintf(buf, sizeof(buf), "%3d%%", micLevel);
  d.fillRect(294, 60, 26, 12, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(294, 60, buf, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  // Peak dB
  float peakDb = box.audio().getMicPeakDb();
  d.fillRect(0, 200, ESP32S3BoxLiteDisplay::Width, 12, ESP32S3BoxLiteDisplay::ColorBlack);
  snprintf(buf, sizeof(buf), "Peak: %.1f dBFS  SR: %lu Hz", (double)peakDb, (unsigned long)box.audio().sampleRate());
  d.drawText(10, 200, buf, 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);

  // Freq and tone info
  d.fillRect(0, 214, ESP32S3BoxLiteDisplay::Width, 12, ESP32S3BoxLiteDisplay::ColorBlack);
  snprintf(buf, sizeof(buf), "Tone: %d Hz  ENT=toggle", toneFreqHz);
  d.drawText(10, 214, buf, 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  d.fillRect(0, 228, ESP32S3BoxLiteDisplay::Width, 12, ESP32S3BoxLiteDisplay::ColorBlack);
  snprintf(buf, sizeof(buf), "Uptime: %lu s", (unsigned long)(millis() / 1000));
  d.drawText(10, 228, buf, 1, ESP32S3BoxLiteDisplay::ColorGreen, ESP32S3BoxLiteDisplay::ColorBlack);
}

void drawSystemPage() {
  auto &d = box.display();

  static DemoPage lastPage = DemoPage::Boot;
  if (lastPage != DemoPage::System) {
    lastPage = DemoPage::System;
    d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawStatusBar("SYSTEM", "Phase 6/7", ESP32S3BoxLiteDisplay::ColorBlue);
  }

  d.fillRect(0, 18, ESP32S3BoxLiteDisplay::Width, ESP32S3BoxLiteDisplay::Height - 18, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Free heap:  %lu bytes", (unsigned long)box.getFreeHeap());
  d.drawText(10, 22, sysLine, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "CPU freq:   %lu MHz", (unsigned long)box.getCpuFreqMHz());
  d.drawText(10, 36, sysLine, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Reset:      %s", box.getResetReason());
  d.drawText(10, 50, sysLine, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Wakeup:     %s", box.getWakeupReason());
  d.drawText(10, 64, sysLine, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  d.fillRect(0, 78, ESP32S3BoxLiteDisplay::Width, 2, ESP32S3BoxLiteDisplay::ColorGray);

  d.drawText(10, 84, "NVS (non-volatile storage):", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  int32_t counter = box.nvsGetInt("boot_cnt", 0) + 1;
  box.nvsSetInt("boot_cnt", counter);
  snprintf(sysLine, sizeof(sysLine), "boot_cnt:   %ld (persists reboots)", (long)counter);
  d.drawText(10, 98, sysLine, 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);

  box.nvsSetStr("page", "System");
  char nvsStr[32] = {};
  box.nvsGetStr("page", nvsStr, sizeof(nvsStr));
  snprintf(sysLine, sizeof(sysLine), "page:       \"%s\"", nvsStr);
  d.drawText(10, 112, sysLine, 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);

  d.fillRect(0, 126, ESP32S3BoxLiteDisplay::Width, 2, ESP32S3BoxLiteDisplay::ColorGray);

  d.drawText(10, 132, "Phase 7 - UI Helpers:", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 146, "showMessage(text, bgColor)", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 160, "showError(text) - red background", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 174, "showBootScreen(appName, version)", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 188, "drawStatusBar(left, right, color)", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);

  d.drawText(10, 204, "ENT=showMessage  CFG=next page", 1, ESP32S3BoxLiteDisplay::ColorGreen, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Uptime: %lu s", (unsigned long)(millis() / 1000));
  d.drawText(10, 218, sysLine, 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
}

}  // namespace

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[ESP32S3BoxLite] PlatformIO Demo starting...");

  box.begin(true, true, false);
  box.input().setButtonCallback(onButton);
  box.input().setLongPressThresholdMs(600);

  // Boot screen
  box.display().showBootScreen("ESP32S3BOX-Lite", "v2.0 PIO");
  delay(1500);

  audioReady = box.audio().begin();
  if (audioReady) {
    box.audio().setSpeakerVolumePercent(volumePercent);
    box.audio().beepPattern(BeepPattern::OK);
    Serial.printf("[Audio] sample rate: %lu Hz\n", (unsigned long)box.audio().sampleRate());
  } else {
    Serial.println("[Audio] init failed");
    box.display().showError("Audio Init Failed");
    delay(2000);
  }

  Serial.printf("[System] Free heap: %lu  CPU: %lu MHz  Reset: %s\n",
                (unsigned long)box.getFreeHeap(),
                (unsigned long)box.getCpuFreqMHz(),
                box.getResetReason());

  currentPage = DemoPage::Boot;
  pageChanged = true;
}

void loop() {
  const uint32_t now = millis();

  // Drive extended button events (calls callback)
  box.input().pollButtonEventEx();

  // Double-click ENT -> OK beep
  if (box.input().isDoubleClick(ESP32S3BoxLiteButton::Enter) && audioReady) {
    box.audio().beepPattern(BeepPattern::OK);
  }

  // Periodic refresh for live-data pages
  if (now - lastLoopMs >= 150) {
    lastLoopMs = now;

    if (currentPage == DemoPage::Audio && audioReady) {
      micLevel = box.audio().getMicLevelPercent();
      drawAudioPage();
    }
    if (currentPage == DemoPage::System) {
      drawSystemPage();
    }
    if (currentPage == DemoPage::Sprite) {
      drawSpritePage();
    }
  }

  if (pageChanged) {
    pageChanged = false;
    drawPage();
  }
}
