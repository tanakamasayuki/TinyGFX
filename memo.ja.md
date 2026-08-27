# TinyGFX

Arduino標準API上で動く、軽量・移植容易な組み込み向けGFXライブラリ — コンセプト叩き台

> 一言でいうと： LovyanGFXの「使いやすい描画API」の感覚を保ちつつ、下層はArduino標準APIだけでも動く最小構成にする。高速化は任意のバックエンドとして追加する。

## 1. 背景 / Motivation

- CH32V系など、Arduino Coreは存在するが高機能GFXライブラリの最適化対象になっていないMCUでも、同じ描画コードを使いたい。

- 既存の高機能GFXは便利だが、MCU固有レジスタ、DMA、バス最適化への依存が大きく、未対応MCUへの移植コストが高くなりやすい。

- 一方、単純なSPI LCD用途では、巨大な抽象化やフレームバッファは不要なことも多い。Arduino標準APIを土台に「まず動く」を保証し、必要な環境だけ高速化できる構造が欲しい。

## 2. コンセプト

| 原則 | 内容 |
| --- | --- |
| Arduino First | 標準実装は pinMode / digitalWrite / delay / SPI / Print など、Arduino Coreが提供するAPIだけで成立させる。 |
| Small Core | 描画コアは小さく保ち、RGB565 + SPI LCDを最初の主戦場にする。 |
| Fast Path Optional | 速度が必要なMCUだけ、GPIO/SPI/DMA直叩きのFast Backendを差し込める。 |
| Familiar API | fillScreen / drawPixel / fillRect / pushImage / setCursor / print など、既存GFXユーザーが迷いにくいAPIを優先する。 |
| No Framebuffer Required | 基本はディスプレイへ直接ストリーム描画。フルフレームバッファを前提にしない。 |
| Portable by Design | PanelとBusを分離し、MCU・LCDコントローラの組み合わせ爆発を避ける。 |

## 3. 目指さないこと / Non-goals

- LovyanGFX完全互換を最初から目指さない。互換性より、小ささ・読みやすさ・移植しやすさを優先する。

- 全MCU向けに最高速を保証しない。標準Backendは「正しく動く」、Fast Backendは「必要な環境で速くする」という役割分担にする。

- 初期段階では複雑なフォント、JPEG/PNGデコード、スプライト、レイヤ合成などをコアに含めない。必要なら拡張モジュールに分離する。

## 4. アーキテクチャ案

```text
Application
  |
  v
TinyGFX API  (drawPixel / fillRect / pushImage / print ...)
  |
  +-- Graphics Core (clipping, primitive, text)
  |
  +-- Panel (ST7735 / ST7789 / ILI9341 ...)
          |
          v
        Bus API
          |
          +-- ArduinoSPIBus   <- default / portable
          +-- CH32FastSPIBus  <- optional
          +-- STM32FastSPIBus <- optional
```

最下層のインターフェースは極力狭くする。

```cpp
class Bus {
public:
  void begin();
  void beginWrite();
  void endWrite();
  void writeCommand(uint8_t cmd);
  void writeData(const uint8_t* data, size_t len);
  void writePixels(const uint16_t* rgb565, size_t count);
};
```

## 5. ユーザー向けAPIのイメージ

```cpp
#include <TinyGFX.h>

TinyGFX display(/* pins / config */);

void setup() {
  display.begin();
  display.setRotation(1);
  display.fillScreen(TinyGFX::BLACK);

  display.setCursor(8, 8);
  display.setTextColor(TinyGFX::WHITE);
  display.println("Hello CH32V");

  display.fillRect(10, 40, 80, 20, TinyGFX::GREEN);
}

void loop() {}
```

文字出力は Arduino の Print を継承し、print()/println() を自然に使える形を基本とする。

## 6. MVP（最小リリース）

| 領域 | MVP内容 |
| --- | --- |
| 描画 | drawPixel, drawFastHLine, drawFastVLine, drawRect, fillRect, fillScreen |
| 画像 | pushImage（RGB565）、setAddrWindow / writePixels 相当 |
| 文字 | 固定幅ビットマップフォント1種、setCursor, setTextColor, setTextSize, Print対応 |
| Panel | ST7789を第一候補。次にST7735 / ILI9341。 |
| Bus | Arduino SPIを標準実装。CS/DC/RSTはdigitalWriteで制御。 |
| Color | RGB565をコア表現とする。RGB888変換はヘルパー程度。 |
| Memory | 動的確保なしでも利用可能。フルフレームバッファ不要。 |

## 7. 高速化の考え方

- 標準Backendは可搬性優先：SPI.beginTransaction() / SPI.transfer() を使い、Arduino Coreが対応する限り動く。

- Fast Backendは完全に任意：CH32V向けならSPIレジスタ、GPIO BSHR/BCR、DMAなどを直接使ってよい。

- 上位APIとPanel実装はFast Backendの有無を意識しない。これにより、最適化が未実装のMCUでも機能を失わない。

- 将来的にはコンパイル時選択（template / traits / #ifdef）を使い、仮想関数コストを避ける。

## 8. 名前：TinyGFX

候補としては良い。 「小さい」「GFXライブラリ」「既存のTiny系Arduinoライブラリ群と馴染む」という意味が一目で伝わる。

注意点： TinyGFX / tinygfx という名称は別分野で既に使われた例があるため、公開・Library Registry登録前にGitHub、Arduino Library Registry、PlatformIO Registryで重複確認する。Arduino向けで強い競合名がなければ、そのまま採用でよい。

## 9. 初期ターゲット案

- Primary: CH32V003 / CH32V20x + Arduino Core + SPI LCD

- Reference portable target: ESP32 / RP2040など、Arduino SPIが安定している環境

- First panel: ST7789（240x240 / 240x320あたり）

- Goal: 同じexample sketchが複数MCUでほぼ変更なく動くこと

## 10. 最初に決めたい設計事項

1. 設定方法：コンストラクタ引数中心か、Config structにするか。

1. Bus/Panelの結合：templateで静的に結ぶか、薄い抽象クラスを使うか。

1. 座標型：int16_t固定で十分か。

1. 色型：uint16_tをそのまま使うか、rgb565_tのような型を用意するか。

1. 文字描画：最初からPrint継承を必須にするか。

1. 互換方針：「LovyanGFX風」までに留めるか、一部メソッド名を意図的に合わせるか。

## 11. 仮のロードマップ

| 段階 | 内容 |
| --- | --- |
| v0.1 | ArduinoSPIBus + ST7789 + 基本プリミティブ + RGB565 pushImage |
| v0.2 | Print対応 + 最小フォント + rotation/clipping |
| v0.3 | ST7735 / ILI9341、Panel初期化テーブルの整理 |
| v0.4 | CH32V Fast Backend（GPIO/SPI、必要ならDMA） |
| v0.5 | ベンチマーク、examples、API安定化 |

## 12. プロジェクトの短い説明文（仮）

> TinyGFX is a small, Arduino-first graphics library for SPI displays. It provides a familiar GFX-style API using standard Arduino interfaces by default, while allowing optional MCU-specific fast backends for SPI, GPIO, and DMA.

Status: Concept Draft
