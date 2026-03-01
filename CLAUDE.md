# esp32-s3-box-lite-easy 開発計画

ESP32-S3-BOX-Lite 専用 Arduino ライブラリ。外部ライブラリ不要・使いやすさ重視・ハード完全活用を目標とする。

---

## プロジェクト概要

- メインファイル: `src/ESP32S3BoxLite.h` / `src/ESP32S3BoxLite.cpp`
- サンプル: `examples/FullFunctionDemo/` (Arduino IDE), `examples/PlatformIO_Demo/` (PlatformIO)
- 対象: ESP32-S3-BOX-Lite（固定ピン配線前提）
- ビルド環境: Arduino-ESP32 core 3.x

---

## ハードウェア構成（固定ピン）

| 機能 | ピン |
|---|---|
| LCD SPI MOSI | GPIO6 |
| LCD SPI CLK | GPIO7 |
| LCD CS | GPIO5 |
| LCD DC | GPIO4 |
| LCD RST | GPIO48 |
| LCD Backlight | GPIO45 |
| Button CFG | GPIO0 |
| Button ADC (PRV/ENT/NXT) | GPIO1 |
| I2C SDA | GPIO8 |
| I2C SCL | GPIO18 |
| I2S BCLK | GPIO17 |
| I2S MCLK | GPIO2 |
| I2S WS | GPIO47 |
| I2S DOUT | GPIO15 |
| I2S DIN | GPIO16 |
| Speaker AMP EN | GPIO46 |
| Speaker DAC | ES8156 (I2C 0x10) |
| Mic ADC | ES7243E (I2C 0x20) |

---

## 実装タスク一覧

### Phase 1 - Display 強化（優先度：高）

- [ ] `setBacklight(uint8_t percent)` — GPIO45 を ledc PWM で輝度制御
- [ ] `drawPixel(int16_t x, int16_t y, uint16_t color)` — 1ピクセル描画
- [ ] `drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)` — Bresenham直線
- [ ] `drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)` — 矩形アウトライン
- [ ] `drawCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color)` — 円アウトライン
- [ ] `fillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color)` — 塗りつぶし円
- [ ] `drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t fgColor, uint16_t bgColor)` — モノクロ1bitビットマップ
- [ ] `drawRGBBitmap(int16_t x, int16_t y, const uint16_t* bitmap, int16_t w, int16_t h)` — RGB565ビットマップ
- [ ] `drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t fgColor, uint16_t bgColor)` — プログレスバー
- [ ] `printf(int16_t x, int16_t y, uint8_t scale, uint16_t fg, uint16_t bg, const char* fmt, ...)` — printf書式対応テキスト
- [ ] 小文字フォント追加（現状は大文字+数字のみ）

### Phase 2 - Sprite（ちらつきなし描画）

- [ ] `createSprite(int16_t w, int16_t h)` — PSRAM/DRAM にフレームバッファ確保
- [ ] `deleteSprite()` — バッファ解放
- [ ] `pushSprite(int16_t x, int16_t y)` — スプライトを一括転送（DMA）
- [ ] スプライト上での描画系関数（fillRect/drawText/etc.）

### Phase 3 - Input 強化（優先度：高）

- [ ] 長押し検出: `pollButtonEventEx()` → `PRESSED` / `LONG_PRESSED` / `RELEASED` を返すenum
- [ ] `setLongPressThresholdMs(uint32_t ms)` — 長押し判定時間設定（デフォルト500ms）
- [ ] ダブルクリック検出: `isDoubleClick(ESP32S3BoxLiteButton btn)`
- [ ] コールバック登録: `setButtonCallback(void (*cb)(ESP32S3BoxLiteButton, ButtonEvent))`
- [ ] ブロッキング待機: `waitForButton(uint32_t timeoutMs)` → タイムアウト時は `None` 返却

### Phase 4 - Audio 強化（優先度：高）

- [ ] `playWav(const uint8_t* data, size_t len)` — メモリ上のWAVデータ再生（PCM 16bit mono/stereo対応）
- [ ] `setVolumeFade(uint8_t targetPercent, uint32_t durationMs)` — フェードイン/アウト
- [ ] `muteToggle()` / `setMute(bool mute)` — ミュート制御（GPIO46）
- [ ] `beepPattern(BeepPattern pattern)` — 定型ビープ（OK/ERROR/DOUBLE/TRIPLE）
- [ ] `getMicLevelPercent()` — リアルタイムマイクレベル（ノンブロッキング）
- [ ] `getMicPeakDb()` — dBFS換算レベル
- [ ] サンプルレート変更対応: `setSampleRate(uint32_t hz)`（現状22050固定）

### Phase 5 - 録音・SPIFFS連携

- [ ] `startMicRecord(int16_t* buffer, size_t maxSamples)` — 録音開始
- [ ] `stopMicRecord()` — 録音停止、記録サンプル数を返す
- [ ] `playWavFromSPIFFS(const char* path)` — SPIFFSからWAV再生
- [ ] `saveMicToSPIFFS(const char* path, const int16_t* samples, size_t count)` — WAVファイル保存

### Phase 6 - System ユーティリティ

- [ ] 電源管理:
  - `enterLightSleep(uint32_t wakeupGpioMask)` — ライトスリープ（ボタン復帰）
  - `enterDeepSleep(uint64_t wakeupTimerUs)` — ディープスリープ（タイマー復帰）
  - `getWakeupReason()` — 復帰要因取得
- [ ] NVS（不揮発性設定保存）:
  - `nvsSetInt(const char* key, int32_t value)`
  - `nvsGetInt(const char* key, int32_t defaultValue)`
  - `nvsSetStr(const char* key, const char* value)`
  - `nvsGetStr(const char* key, char* buf, size_t bufLen)`
- [ ] システム情報:
  - `getFreeHeap()` — 空きヒープ取得
  - `getCpuFreqMHz()` — CPUクロック取得
  - `getResetReason()` — リセット要因文字列取得

### Phase 7 - 便利UI関数

- [ ] `showMessage(const char* text, uint16_t bgColor)` — 画面中央に自動フォーマット表示
- [ ] `showError(const char* text)` — 赤背景エラー表示
- [ ] `showBootScreen(const char* appName, const char* version)` — 起動画面テンプレート
- [ ] `drawStatusBar(const char* left, const char* right, uint16_t bgColor)` — 上部ステータスバー

---

## コーディング規約

- 命名: ESP スタイル（`beginXxx`, `getXxx`, `setXxx`, `readXxx`, `writeXxx`）
- 外部ライブラリ依存なし（Arduino-ESP32 core + ESP-IDF ドライバのみ）
- エラー戻り値: `bool`（成功 `true`、失敗 `false`）
- サブシステムは既存の `display()` / `input()` / `audio()` パターンを踏襲
- PSRAM が利用可能な場合はスプライトバッファに使用（`heap_caps_malloc`）

---

## サンプル更新方針

各 Phase 完了後に `examples/FullFunctionDemo/` と `examples/PlatformIO_Demo/` を更新し、新機能を実演するデモを追加する。

---

## 現状の既知制限

- フォント: 大文字A-Z + 数字 + 一部記号のみ（小文字なし）
- バックライト: ON/OFF のみ（輝度固定）
- ボタン: 押下/離上のみ（長押し・ダブルクリック未対応）
- WAV再生: 未実装（ビープ生成のみ）
- スリープ: 未実装
- NVS: 未実装
