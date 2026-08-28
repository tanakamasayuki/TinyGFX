# examples

> English: [README.md](README.md)

| 例 | 内容 | CH32V003 でのフラッシュ増分 |
| --- | --- | --- |
| [HelloWorld](HelloWorld) | 矩形と文字。**最初に見るのはこれ** | 約 6.0 KB（文字まで） |
| [Shapes](Shapes) | 使えるプリミティブを一通り | 約 4.9 KB |
| [FlickerFree](FlickerFree) | 帯レンダリングでちらつきを消す | 約 7.6 KB / RAM 約 2.0 KB |
| [HardwareSPI](HardwareSPI) | ハードウェア SPI を使う版 | Shapes とほぼ同じ |
| [OledI2C](OledI2C) | **I2C のモノクロ OLED**（SSD1306） | 約 5.6 KB / **RAM 約 1.1 KB** |
| [M5StackBasic](M5StackBasic) | **実機の立ち上げ用**。M5Stack Core / BASIC 専用 | —（ESP32 なので測っていない） |

増分の根拠は [../docs/FOOTPRINT.ja.md](../docs/FOOTPRINT.ja.md)。
**使わない機能はフラッシュに載らない**ので、必要なものだけ呼べばよい。

## 実機で最初に試すなら M5StackBasic

[M5StackBasic](M5StackBasic) だけは**配線が要らない**。ピンが決まっていて電源も
安定しているので、「ライブラリが動くのか、配線が悪いのか」で悩まずに済む。
枠・色順・文字の向き・図形を 1 画面に出し、違っていたときの直し方も画面と
シリアルに書いてある。手順は [../docs/MANUAL_TEST.ja.md](../docs/MANUAL_TEST.ja.md) の M0。

```
arduino-cli compile --profile m5stack -u -p /dev/ttyUSB0 examples/M5StackBasic
```

## モノクロ OLED は勝手が違う

[OledI2C](OledI2C) だけ 2 点ちがう。

1. **フレームバッファが要る。** 128x64 で 1,024 バイト。利用者が用意する
   （128x32 のパネルなら 512 バイト）
2. **`panel.display()` を呼ぶまで画面は変わらない。** 変更のあったページだけ流すので、
   1 ページ（縦 8 画素）に収まる変更なら 128 バイトで済む

描画 API はカラーパネルと同じ。色は「0 でなければ点灯」で 1bpp に落ちる。

## 配線

どの例も先頭でピンを宣言している。手元の配線に合わせて変えること。

| 信号 | 役割 |
| --- | --- |
| SCK / MOSI | SPI。ハードウェア SPI 版では宣言しない（Core に任せる） |
| DC | コマンドとデータの切り替え |
| CS | チップセレクト。専有しているなら -1 でもよい |
| RST | リセット。モジュール側で処理していれば -1 |

## フォントはスケッチ側に置く

**TinyGFX はフォントデータを 1 バイトも同梱していない。**
`HelloWorld` には `tinygfx_font5x7.h` を同梱してあるが、これはつなぎの
5x7（0x20-0x3F の 32 文字）で、実運用のフォントは
[LGFXFontToolJs](https://www.npmjs.com/package/lgfx-font-tool) で作る。

形式は **CellFont**（外部仕様 v1）と **u8g2**。使わなかった形式のデコーダはリンクされない。

生成されたヘッダは**純粋な CellFont** なので、`setFont()` に渡すには 1 行包む:

```cpp
#include <TinyGFX/FontCell.h>
#include "font.h"

static const TinyGFXFontRef myFont = {&Name, &tinygfxFontCellOps, nullptr};
```

**AVR では PROGMEM に置くこと。** 置かないと RAM を食い、絵も化ける。

## パネルの原点オフセット

240x240 や 135x240 の ST7789 モジュールは、コントローラの GRAM より小さいので
原点がずれている。その場合は 2 つとも設定する。

```cpp
panel.setGramSize(240, 320);   // コントローラの GRAM
panel.setOffset(52, 40);       // 回転 0 のときの可視域の位置
```

回転 1〜3 のぶんは自動で導出される。
