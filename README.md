# esp32-s3-box-lite-easy

ESP32-S3-BOX-Lite 用の使いやすさ重視 Arduino ライブラリです。GitHub から ZIP をダウンロードして、そのまま Arduino IDE に追加できる構成です。

## できること

- LCD 表示
- CONFIG ボタン
- ADC 3 ボタン
- スピーカー
- マイク

## 特徴

- 追加の外部ライブラリなしで動作
- ESP32 Arduino core に含まれる `SPI`, `driver/i2c.h`, `driver/i2s.h` を使用
- LCD は ST7789 を直接制御
- ボタンは `GPIO0` と `GPIO1` の ADC 電圧で判定
- 音声は ES8156 と ES7243E を直接初期化
- 画面向きは 180 度を標準設定

## 対応環境

- Arduino IDE
- Arduino-ESP32 core 3.x
- ESP32-S3 系ターゲット

BOX-Lite 専用の固定ピン配線を前提にしています。Arduino IDE に `ESP32-S3-Box` が無い場合でも、`ESP32S3 Dev Module` で利用できます。

## インストール

### Arduino IDE

1. GitHub からこのリポジトリを ZIP でダウンロード
2. `Sketch > Include Library > Add .ZIP Library...`
3. ダウンロードした ZIP を選択

### 手動配置

1. このリポジトリを展開
2. フォルダ名を `ESP32S3BoxLite` にする
3. Arduino の `libraries` フォルダへ配置

## ハードウェア仕様

### LCD

- 解像度: `320x240`
- 接続: SPI
- 向き: 180 度

### ボタン

- `CONFIG`: `GPIO0`
- `PREV / ENTER / NEXT`: `GPIO1` の ADC 分圧

ADC しきい値:

- `NEXT`: `720mV - 920mV`
- `ENTER`: `1880mV - 2080mV`
- `PREV`: `2310mV - 2510mV`

### オーディオ

- スピーカー DAC: `ES8156`
- マイク ADC: `ES7243E`
- I2S サンプルレート: `22050 Hz`
- サンプル形式: `16-bit mono`

## 使い方

```cpp
#include <ESP32S3BoxLite.h>

ESP32S3BoxLite box;

void setup() {
  box.begin();
  box.display().fillScreen(ESP32S3BoxLiteDisplay::ColorBlue);
  box.display().drawTextCentered(40, "HELLO", 4,
                                 ESP32S3BoxLiteDisplay::ColorWhite,
                                 ESP32S3BoxLiteDisplay::ColorBlue);
  box.audio().playBeep(660, 120);
}

void loop() {
  ESP32S3BoxLiteButton button = box.input().pollButtonEvent();
  if (button == ESP32S3BoxLiteButton::Config) {
    box.audio().playBeep(880, 120);
  }
}
```

## API

### 初期化

```cpp
box.begin();
box.begin(true, true, false);
```

### 表示

```cpp
box.display().fillScreen(ESP32S3BoxLiteDisplay::ColorBlue);
box.display().fillRect(20, 20, 100, 40, ESP32S3BoxLiteDisplay::ColorBlack);
box.display().drawText(24, 28, "TEXT", 2,
                       ESP32S3BoxLiteDisplay::ColorWhite,
                       ESP32S3BoxLiteDisplay::ColorBlack);
```

### ボタン

```cpp
ESP32S3BoxLiteButton button = box.input().pollButtonEvent();
const char* name = ESP32S3BoxLiteInput::buttonName(button);
int adcMv = box.input().lastAdcMilliVolts();
```

### スピーカー

```cpp
box.audio().setSpeakerVolumePercent(70);
box.audio().playBeep(660, 120);
```

### マイク

```cpp
int16_t samples[256];
size_t count = box.audio().readMicSamples(samples, 256);
int level = box.audio().samplesToLevelPercent(samples, count);
```

### ループバック

```cpp
int16_t samples[256];
size_t count = box.audio().readMicSamples(samples, 256);
box.audio().writeSpeakerSamples(samples, count);
```

## サンプル

Arduino IDE 用:

- `examples/FullFunctionDemo/FullFunctionDemo.ino`

PlatformIO 用:

- `examples/PlatformIO_Demo/src/main.cpp`

どちらも以下を確認できます。

- LCD 表示
- 全ボタン入力
- ビープ音
- マイクレベルメータ
- マイクループバック

### FullFunctionDemo の操作

- `CFG`: ビープ再生
- `PRV`: ビープ周波数切替
- `ENT`: マイクレベルメータ ON/OFF
- `NXT`: マイクループバック ON/OFF

## 注意

- ESP32-S3-BOX-Lite の固定配線を前提にしています
- 他ボードへそのままは使えません
- 音声入出力は `22050 Hz / 16-bit mono` を前提にしています
