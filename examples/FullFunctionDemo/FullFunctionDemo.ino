// FullFunctionDemo.ino
// Demonstrates all features of the ESP32S3BoxLite library (Phases 1-7).
//
// Controls:
//   CFG button        -> Cycle through demo pages
//   CFG long press    -> Play beep pattern
//   NXT               -> Next item / increase value
//   PRV               -> Previous item / decrease value
//   ENT               -> Select / toggle
//   ENT double-click  -> Play WAV pattern (BeepPattern::OK)

#include <ESP32S3BoxLite.h>

namespace {

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

ESP32S3BoxLite box;

// Demo pages
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

// Audio state
uint8_t  volumePercent = 65;
int      toneFreqHz    = 440;
int      micLevel      = 0;

// System state
char sysLine[64];

// Sprite demo
ESP32S3BoxLiteSprite sprite;
bool spriteCreated = false;
int  spriteAngle   = 0;

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

// ---------------------------------------------------------------------------
// Button callback (Phase 3)
// ---------------------------------------------------------------------------

void onButton(ESP32S3BoxLiteButton btn, ButtonEvent evt) {
  handleButton(btn, evt);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("ESP32S3BoxLite FullFunctionDemo starting...");

  // Init display and input first (audio separately for error display)
  box.begin(true, true, false);
  box.input().setButtonCallback(onButton);
  box.input().setLongPressThresholdMs(600);

  // Boot screen (Phase 7)
  box.display().showBootScreen("ESP32S3BOX-Lite", "v2.0 Demo");
  delay(1500);

  // Now init audio
  audioReady = box.audio().begin();
  if (audioReady) {
    box.audio().setSpeakerVolumePercent(volumePercent);
    box.audio().beepPattern(BeepPattern::OK);
  } else {
    box.display().showError("Audio Init Failed");
    delay(2000);
  }

  currentPage = DemoPage::Boot;
  pageChanged = true;
}

void loop() {
  const uint32_t now = millis();

  // Poll extended button events (drives the callback too)
  box.input().pollButtonEventEx();

  // Double-click detection for ENT -> play OK pattern
  if (box.input().isDoubleClick(ESP32S3BoxLiteButton::Enter) && audioReady) {
    box.audio().beepPattern(BeepPattern::OK);
  }

  // Refresh display for pages with live data
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

namespace {

// ---------------------------------------------------------------------------
// Page dispatcher
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

void nextPage() {
  int p = static_cast<int>(currentPage) + 1;
  if (p >= static_cast<int>(DemoPage::kCount)) { p = 1; }  // skip Boot
  currentPage = static_cast<DemoPage>(p);
  pageChanged = true;
}

// ---------------------------------------------------------------------------
// Button handler
// ---------------------------------------------------------------------------

void handleButton(ESP32S3BoxLiteButton btn, ButtonEvent evt) {
  if (evt == ButtonEvent::Pressed) {
    if (btn == ESP32S3BoxLiteButton::Config) {
      nextPage();
      if (audioReady) { box.audio().playBeep(880, 80); }
    } else if (btn == ESP32S3BoxLiteButton::Next) {
      if (currentPage == DemoPage::Audio) {
        volumePercent = (volumePercent < 100) ? volumePercent + 10 : 100;
        box.audio().setSpeakerVolumePercent(volumePercent);
      }
      pageChanged = true;
    } else if (btn == ESP32S3BoxLiteButton::Prev) {
      if (currentPage == DemoPage::Audio) {
        volumePercent = (volumePercent > 10) ? volumePercent - 10 : 10;
        box.audio().setSpeakerVolumePercent(volumePercent);
      }
      pageChanged = true;
    } else if (btn == ESP32S3BoxLiteButton::Enter) {
      if (currentPage == DemoPage::Audio && audioReady) {
        toneFreqHz = (toneFreqHz == 440) ? 880 : 440;
        box.audio().playBeep(toneFreqHz, 120);
      }
      pageChanged = true;
    }
  }
  // Long press on Config -> play error pattern
  if (evt == ButtonEvent::LongPressed && btn == ESP32S3BoxLiteButton::Config && audioReady) {
    box.audio().beepPattern(BeepPattern::Error);
  }
}

// ---------------------------------------------------------------------------
// Page: Boot (summary)
// ---------------------------------------------------------------------------

void drawBootPage() {
  auto &d = box.display();
  d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawStatusBar("ESP32S3BOX-Lite", "DEMO", ESP32S3BoxLiteDisplay::ColorBlue);

  d.drawTextCentered(30, "FULL FUNCTION", 2, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawTextCentered(52, "DEMO v2.0", 2, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  d.drawText(10, 90,  "CFG = next page", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 104, "NXT/PRV = adjust", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 118, "ENT = select", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);

  // Pages overview
  d.drawText(10, 140, "Pages:", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 152, "Display  Primitives  Sprite", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 164, "Input  Audio  System", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Heap: %lu B  CPU: %lu MHz",
           (unsigned long)box.getFreeHeap(),
           (unsigned long)box.getCpuFreqMHz());
  d.drawText(10, 200, sysLine, 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  const char *resetStr = box.getResetReason();
  snprintf(sysLine, sizeof(sysLine), "Reset: %s", resetStr);
  d.drawText(10, 214, sysLine, 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Audio: %s", audioReady ? "OK" : "FAIL");
  d.drawText(10, 228, sysLine, 1, audioReady ? ESP32S3BoxLiteDisplay::ColorGreen : ESP32S3BoxLiteDisplay::ColorRed,
             ESP32S3BoxLiteDisplay::ColorBlack);
}

// ---------------------------------------------------------------------------
// Page: Display features (Phase 1)
// ---------------------------------------------------------------------------

void drawDisplayPage() {
  auto &d = box.display();
  d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawStatusBar("DISPLAY", "Phase 1", ESP32S3BoxLiteDisplay::ColorBlue);

  // Backlight control demo (set to 80% for this page)
  d.setBacklight(80);

  // drawText with lowercase (new in font)
  d.drawText(10, 24, "Uppercase ABCDEFG", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 36, "Lowercase abcdefg", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 48, "Numbers 0123456789", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);

  // printf (Phase 1)
  d.printf(10, 68, 1, ESP32S3BoxLiteDisplay::ColorGreen, ESP32S3BoxLiteDisplay::ColorBlack,
           "printf: heap=%lu", (unsigned long)box.getFreeHeap());

  // Progress bars (Phase 1)
  d.drawText(10, 88, "Progress bars:", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawProgressBar(10, 100, 300, 12, 25,  ESP32S3BoxLiteDisplay::ColorRed,    ESP32S3BoxLiteDisplay::ColorGray);
  d.drawProgressBar(10, 116, 300, 12, 50,  ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorGray);
  d.drawProgressBar(10, 132, 300, 12, 75,  ESP32S3BoxLiteDisplay::ColorGreen,  ESP32S3BoxLiteDisplay::ColorGray);
  d.drawProgressBar(10, 148, 300, 12, 100, ESP32S3BoxLiteDisplay::ColorCyan,   ESP32S3BoxLiteDisplay::ColorGray);

  // Color swatches
  d.drawText(10, 174, "Colors:", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  constexpr uint16_t colors[] = {
    ESP32S3BoxLiteDisplay::ColorRed,    ESP32S3BoxLiteDisplay::ColorGreen,
    ESP32S3BoxLiteDisplay::ColorBlue,   ESP32S3BoxLiteDisplay::ColorCyan,
    ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorOrange,
    ESP32S3BoxLiteDisplay::ColorPurple, ESP32S3BoxLiteDisplay::ColorWhite,
  };
  for (int i = 0; i < 8; ++i) {
    d.fillRect(static_cast<uint16_t>(10 + i * 38), 186, 34, 20, colors[i]);
  }

  d.drawText(10, 218, "CFG=next page  BL=80%", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  d.setBacklight(100);  // restore
}

// ---------------------------------------------------------------------------
// Page: Drawing primitives (Phase 1)
// ---------------------------------------------------------------------------

void drawPrimitivesPage() {
  auto &d = box.display();
  d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawStatusBar("PRIMITIVES", "Phase 1", ESP32S3BoxLiteDisplay::ColorBlue);

  // Pixels
  for (int i = 0; i < 20; ++i) {
    d.drawPixel(10 + i * 2, 30, ESP32S3BoxLiteDisplay::ColorWhite);
  }

  // Lines
  d.drawLine(10, 45, 200, 45, ESP32S3BoxLiteDisplay::ColorRed);
  d.drawLine(10, 55, 200, 75, ESP32S3BoxLiteDisplay::ColorGreen);
  d.drawLine(200, 55, 10, 75, ESP32S3BoxLiteDisplay::ColorBlue);

  // Rectangles (outline)
  d.drawRect(10,  90, 80, 40, ESP32S3BoxLiteDisplay::ColorCyan);
  d.drawRect(100, 90, 80, 40, ESP32S3BoxLiteDisplay::ColorYellow);
  d.drawRect(200, 90, 80, 40, ESP32S3BoxLiteDisplay::ColorOrange);

  // Circles
  d.drawCircle(40,  160, 25, ESP32S3BoxLiteDisplay::ColorRed);
  d.drawCircle(110, 160, 25, ESP32S3BoxLiteDisplay::ColorGreen);
  d.fillCircle(180, 160, 25, ESP32S3BoxLiteDisplay::ColorBlue);
  d.fillCircle(250, 160, 25, ESP32S3BoxLiteDisplay::ColorPurple);

  // 1-bit bitmap (a simple 8x8 checkerboard pattern)
  static const uint8_t kChecker[] = {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
  d.drawBitmap(10, 200, kChecker, 8, 8, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorGray);

  // RGB565 bitmap (small 4x4 gradient)
  static const uint16_t kRgb[] = {
    0xF800, 0xF810, 0xF820, 0xF830,
    0x07E0, 0x07F0, 0x0800, 0x0810,
    0x001F, 0x041F, 0x081F, 0x0C1F,
    0xFFE0, 0xFFE8, 0xFFF0, 0xFFF8,
  };
  d.drawRGBBitmap(30, 200, kRgb, 4, 4);

  d.drawText(10, 228, "Pixels Lines Rects Circles Bitmaps", 1,
             ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
}

// ---------------------------------------------------------------------------
// Page: Sprite (Phase 2)
// ---------------------------------------------------------------------------

static constexpr float kPiLocal = 3.14159265f;

void drawSpritePage() {
  auto &d = box.display();

  if (!spriteCreated) {
    d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawStatusBar("SPRITE", "Phase 2", ESP32S3BoxLiteDisplay::ColorBlue);
    d.drawTextCentered(120, "Creating sprite...", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

    spriteCreated = sprite.createSprite(200, 150);
    if (!spriteCreated) {
      d.showError("Sprite alloc failed");
      return;
    }
  }

  // Draw into sprite
  sprite.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);

  // Animated rectangle border
  sprite.drawText(2, 2, "Sprite Demo", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);

  // Draw some geometry into sprite
  const int16_t cx = 100, cy = 80, r = 40 + (spriteAngle % 20) - 10;
  for (int i = 0; i < 200; i += 4) {
    const int16_t sx = static_cast<int16_t>(cx + r * cosf(static_cast<float>(i + spriteAngle) * kPiLocal / 100.0f));
    const int16_t sy = static_cast<int16_t>(cy + r * sinf(static_cast<float>(i + spriteAngle) * kPiLocal / 100.0f));
    sprite.drawPixel(sx, sy, ESP32S3BoxLiteDisplay::ColorGreen);
  }

  sprite.fillRect(0, 130, 200, 20, ESP32S3BoxLiteDisplay::ColorBlue);
  sprite.drawText(2, 133, "pushSprite -> no flicker", 1,
                  ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlue);

  sprite.pushSprite(d, 60, 44);

  // Status below sprite
  d.printf(10, 200, 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack,
           "Sprite %dx%d  angle=%d", sprite.width(), sprite.height(), spriteAngle);
  d.drawText(10, 214, "Smooth animation without tearing", 1,
             ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);

  spriteAngle = (spriteAngle + 3) % 200;
}

// ---------------------------------------------------------------------------
// Page: Input (Phase 3)
// ---------------------------------------------------------------------------

void drawInputPage() {
  auto &d = box.display();
  d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawStatusBar("INPUT", "Phase 3", ESP32S3BoxLiteDisplay::ColorBlue);

  d.drawText(10, 28,  "Button events (pollButtonEventEx):", 1,
             ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  // Describe each button and its state
  constexpr struct { ESP32S3BoxLiteButton btn; const char *name; int16_t y; } kBtns[] = {
    {ESP32S3BoxLiteButton::Config, "CFG - next page (long=error beep)", 44},
    {ESP32S3BoxLiteButton::Prev,   "PRV - volume down",                  58},
    {ESP32S3BoxLiteButton::Enter,  "ENT - toggle / dbl-click OK beep",   72},
    {ESP32S3BoxLiteButton::Next,   "NXT - volume up",                     86},
  };
  for (auto &b : kBtns) {
    d.drawText(10, b.y, b.name, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
  }

  d.drawText(10, 106, "Long press threshold: 600 ms", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 120, "Double-click window:  400 ms", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);

  d.drawText(10, 140, "waitForButton demo (1s timeout):", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 154, "Press any button now...", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  ESP32S3BoxLiteButton pressed = box.input().waitForButton(1000);
  char msg[32];
  snprintf(msg, sizeof(msg), "Got: %s", ESP32S3BoxLiteInput::buttonName(pressed));
  d.fillRect(10, 168, 300, 12, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 168, msg, 1, ESP32S3BoxLiteDisplay::ColorGreen, ESP32S3BoxLiteDisplay::ColorBlack);

  d.drawText(10, 192, "ADC mV:", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  snprintf(msg, sizeof(msg), "%d mV", box.input().lastAdcMilliVolts());
  d.drawText(60, 192, msg, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
}

// ---------------------------------------------------------------------------
// Page: Audio (Phase 4)
// ---------------------------------------------------------------------------

void drawAudioPage() {
  auto &d = box.display();

  // Only redraw static elements on first entry
  static DemoPage lastDrawnPage = DemoPage::Boot;
  if (lastDrawnPage != DemoPage::Audio) {
    lastDrawnPage = DemoPage::Audio;
    d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawStatusBar("AUDIO", "Phase 4", ESP32S3BoxLiteDisplay::ColorBlue);
    d.drawText(10, 28, "NXT=vol+  PRV=vol-  ENT=beep", 1,
               ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 44, "Volume:", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 72, "Mic level:", 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 100, "Patterns (CFG long=Error):", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 114, "OK  Error  Double  Triple", 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawText(10, 136, "Fade demo:", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  }

  if (!audioReady) {
    d.drawText(10, 160, "Audio not ready!", 1, ESP32S3BoxLiteDisplay::ColorRed, ESP32S3BoxLiteDisplay::ColorBlack);
    return;
  }

  // Volume bar
  d.drawProgressBar(10, 56, 300, 12, volumePercent, ESP32S3BoxLiteDisplay::ColorGreen, ESP32S3BoxLiteDisplay::ColorGray);
  char vol[24];
  snprintf(vol, sizeof(vol), "%3d%%", volumePercent);
  d.drawText(280, 44, vol, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  // Mic level bar
  d.drawProgressBar(10, 84, 300, 12, static_cast<uint8_t>(micLevel), ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorGray);
  char lvl[24];
  snprintf(lvl, sizeof(lvl), "%3d%%", micLevel);
  d.drawText(280, 72, lvl, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  // Peak dB
  float peakDb = box.audio().getMicPeakDb();
  d.fillRect(10, 152, 300, 12, ESP32S3BoxLiteDisplay::ColorBlack);
  char dbLine[32];
  snprintf(dbLine, sizeof(dbLine), "Peak: %.1f dBFS  SR: %lu Hz", (double)peakDb, (unsigned long)box.audio().sampleRate());
  d.drawText(10, 152, dbLine, 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);

  // Fade demo line
  d.fillRect(10, 148, 300, 12, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 148, "ENT=play beep  dbl-ENT=fade demo", 1,
             ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  // Sample rate info
  d.fillRect(10, 170, 300, 12, ESP32S3BoxLiteDisplay::ColorBlack);
  snprintf(dbLine, sizeof(dbLine), "Muted: %s  Freq: %d Hz", "no", toneFreqHz);
  d.drawText(10, 170, dbLine, 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
}

// ---------------------------------------------------------------------------
// Page: System (Phase 6)
// ---------------------------------------------------------------------------

void drawSystemPage() {
  auto &d = box.display();

  static DemoPage lastSysPage = DemoPage::Boot;
  if (lastSysPage != DemoPage::System) {
    lastSysPage = DemoPage::System;
    d.fillScreen(ESP32S3BoxLiteDisplay::ColorBlack);
    d.drawStatusBar("SYSTEM", "Phase 6", ESP32S3BoxLiteDisplay::ColorBlue);
  }

  d.fillRect(0, 20, ESP32S3BoxLiteDisplay::Width, ESP32S3BoxLiteDisplay::Height - 20, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Free heap:  %lu bytes", (unsigned long)box.getFreeHeap());
  d.drawText(10, 28, sysLine, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "CPU freq:   %lu MHz", (unsigned long)box.getCpuFreqMHz());
  d.drawText(10, 42, sysLine, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Reset:      %s", box.getResetReason());
  d.drawText(10, 56, sysLine, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  snprintf(sysLine, sizeof(sysLine), "Wakeup:     %s", box.getWakeupReason());
  d.drawText(10, 70, sysLine, 1, ESP32S3BoxLiteDisplay::ColorWhite, ESP32S3BoxLiteDisplay::ColorBlack);

  // NVS demo
  d.drawText(10, 90, "NVS (non-volatile storage):", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);

  // Write and read back a counter
  int32_t counter = box.nvsGetInt("boot_cnt", 0) + 1;
  box.nvsSetInt("boot_cnt", counter);
  snprintf(sysLine, sizeof(sysLine), "boot_cnt:   %ld", (long)counter);
  d.drawText(10, 104, sysLine, 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);

  box.nvsSetStr("last_page", "System");
  char nvsStr[32] = {};
  box.nvsGetStr("last_page", nvsStr, sizeof(nvsStr));
  snprintf(sysLine, sizeof(sysLine), "last_page:  \"%s\"", nvsStr);
  d.drawText(10, 118, sysLine, 1, ESP32S3BoxLiteDisplay::ColorCyan, ESP32S3BoxLiteDisplay::ColorBlack);

  // showMessage demo button
  d.drawText(10, 142, "Phase 7 - UI Helpers:", 1, ESP32S3BoxLiteDisplay::ColorGray, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 156, "ENT = showMessage demo", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 170, "NXT = showError demo", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);
  d.drawText(10, 184, "PRV = showBootScreen demo", 1, ESP32S3BoxLiteDisplay::ColorYellow, ESP32S3BoxLiteDisplay::ColorBlack);

  // Display uptime
  snprintf(sysLine, sizeof(sysLine), "Uptime: %lu s", (unsigned long)(millis() / 1000));
  d.drawText(10, 210, sysLine, 1, ESP32S3BoxLiteDisplay::ColorGreen, ESP32S3BoxLiteDisplay::ColorBlack);
}

}  // namespace
