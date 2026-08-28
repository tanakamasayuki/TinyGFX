# TinyGFX

**LovyanGFX に似た書き味の描画 API を、フラッシュと RAM が極端に少ない MCU でも載るところまで削ったライブラリ。**

> English: [README.md](README.md)

CH32V003（フラッシュ 16KB / RAM 2KB）で、**全機能を使っても +6.5KB**。
使っていない機能は 1 バイトも載りません。

> ### 実機で動いたのは M5Stack だけです
>
> **2026-08-28、M5Stack BASIC（ILI9342C）で初めて実機に絵が出ました。** ホスト上のテストも
> **ホストで 45 本、実機で 1 本**通っています。ただし**実物のディスプレイで確かめたのはこの 1 構成だけ**です。
>
> | | |
> | --- | --- |
> | 確認済み | ILI9342C + ハードウェア SPI（ESP32）、描画プリミティブ、文字、回転 0、**実機の絵がホストと一致** |
> | **未確認** | **ST7789 / SSD1306 / ソフト SPI / I2C / CH32V003 / 回転 1〜3 / 帯レンダリング** |
>
> **実機の自動テストも通っています** — M5Stack の上で描いた絵が、ホストで作った
> ゴールデンと 1 画素も違わないことを毎回確かめています（[tests/hw/m5stack/](tests/hw/m5stack/)）。
>
> 試すなら **配線の要らない [examples/M5StackBasic](examples/M5StackBasic)** が一番確実です。
> 枠・色順・文字の向き・図形を 1 画面に出し、違っていたときの直し方も画面とシリアルに
> 書いてあります。手順は [docs/MANUAL_TEST.ja.md](docs/MANUAL_TEST.ja.md) の M0。
> API もまだ動く可能性があります。

## なにが違うのか

| | |
| --- | --- |
| **使わない機能は 0 バイト** | `fillScreen` しか呼ばないスケッチに円や文字は載りません。**テストで機械的に検査しています** |
| **フレームバッファ不要** | 画面へ直接流します。要るときだけ帯レンダリング（下記）で持ちます |
| **動的確保なし** | `malloc` / `new` / `String` を使いません。バッファは利用者が渡します |
| **バス・パネル・フォント形式が差し替え可能** | しかも**使っていない実装はリンクされません** |
| **数字で決めている** | 設計判断はすべて CH32V003 の実測値が根拠です（[docs/FOOTPRINT.ja.md](docs/FOOTPRINT.ja.md)） |

## 使ってみる

### SPI のカラー TFT（ST7789）

```cpp
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/PanelST7789.h>

TinyGFXBusSoftSPI  bus(/*sck*/5, /*mosi*/6, /*dc*/3, /*cs*/4);
TinyGFXPanelST7789 panel(bus, 240, 240, /*rst*/2);
TinyGFX            lcd(panel);

void setup() {
  lcd.begin();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BLACK);
  lcd.fillRect(10, 40, 80, 20, TFT_GREEN);
}
void loop() {}
```

既定のバスが**ソフト SPI（ビットバン）**なのは、CH32V003 の Arduino Core に SPI ライブラリが
無いためです。ハードウェア SPI が使える環境では `<TinyGFX/BusSPI.h>` に差し替えるだけで、
描画側は 1 行も変わりません。

### I2C のモノクロ OLED（SSD1306）

```cpp
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/PanelSSD1306.h>

static uint8_t fb[128 * 64 / 8];        // 1,024 バイト。利用者が用意する

TinyGFXBusI2C       bus(/*address*/0x3C);
TinyGFXPanelSSD1306 panel(bus, fb, 128, 64);
TinyGFX             lcd(panel);

void setup() {
  lcd.begin();
  lcd.fillRect(8, 8, 40, 16, TFT_WHITE);
  panel.display();                      // ここで初めて転送される
}
void loop() {}
```

モノクロパネルだけ 2 点ちがいます — **フレームバッファが要る**ことと、
**`display()` を呼ぶまで画面が変わらない**こと（変更のあったページだけ流します）。
描画 API はカラーパネルと同じで、色は「0 でなければ点灯」で 1bpp に落ちます。

もっと見るなら [examples/](examples/)。

## 入っているもの

| 種類 | 実装 | ヘッダ |
| --- | --- | --- |
| バス | ソフト SPI（既定・可搬） | `TinyGFX/BusSoftSPI.h` |
| | ハードウェア SPI | `TinyGFX/BusSPI.h` |
| | I2C（Wire） | `TinyGFX/BusI2C.h` |
| | コマンド列の記録（検証用） | `TinyGFX/BusCapture.h` |
| パネル | ST7789（カラー TFT） | `TinyGFX/PanelST7789.h` |
| | ILI9342C（M5Stack Core / BASIC） | `TinyGFX/PanelILI9342.h` |
| | SSD1306（モノクロ OLED） | `TinyGFX/PanelSSD1306.h` |
| | RAM バッファ（テスト・帯用） | `TinyGFX/PanelMemory.h` |
| フォント | CellFont（H≤16 向けの外部仕様 v1） | `TinyGFX/FontCell.h` |
| | u8g2 | `TinyGFX/FontU8g2.h` |
| 拡張 | 帯レンダリング（ちらつき対策） | `TinyGFX/TileCanvas.h` |
| | `print` / `printf` / float | `TinyGFX/Print.h` |

**include していないものはリンクされません。** バスもパネルもフォント形式も同じです。

## 描けるもの

```
drawPixel  drawFastHLine  drawFastVLine  drawLine  drawRect  fillRect  fillScreen  clear
drawCircle  fillCircle  drawRoundRect  fillRoundRect  drawTriangle  fillTriangle
pushImage（透過版あり）  setAddrWindow  writeColor  writePixels
setClipRect  setRotation  startWrite / endWrite
setFont  setCursor  setTextColor  setTextSize  drawChar  drawString  textWidth  fontHeight
```

名前は決めの問題でしかないところを LovyanGFX に寄せてあります。**互換レイヤではありません。**

## フットプリント（CH32V003 / `-Os` / 実測）

空スケッチが 5,892 バイトなので、**そこからの増分**で管理しています。

| 積み上げ | Δ フラッシュ | Δ RAM |
| --- | --- | --- |
| バス + パネル + `fillScreen` | +1,712 | +68 |
| + 矩形・点・水平垂直線 | +1,988 | +68 |
| + 全プリミティブ（線・円・角丸・三角） | +4,880 | +68 |
| + 文字 | +5,916 | +68 |
| **+ `pushImage`（全機能）** | **+6,536** | **+68** |
| + 帯レンダリング（240px × 1 行） | +7,524 | +624 |
| + `print` / `println`（float なし） | +6,200 | +80 |
| + `println(float)` | **載りません**（約 +8,650） | — |

`println(float)` が載らないのは仕様ではなく実測です。浮動小数点の書式化だけで
CH32V003 のフラッシュの半分を超えます。**禁止はしていません** — 使いたい人が
`TinyGFX/Print.h` を取り込めば使えますし、使わない人は払いません。

対応を確認している環境: **CH32V003**（基準機）、**Arduino Uno R3**、**ESP32**。
他の数字は [docs/FOOTPRINT.ja.md](docs/FOOTPRINT.ja.md)。

## ちらつき対策

フレームバッファが無いので、消してから描くとちらつきます。全画面バッファは
240x240 RGB565 で 115KB あって載りません。そこで**画面を横帯に分け、小さな RAM バッファへ
1 帯ずつ描いてから転送**します。

```cpp
static uint16_t band[240 * 2];          // 幅 × 行数 × 2 バイト
TinyGFXTileCanvas canvas(panel, band, sizeof(band) / sizeof(band[0]));

static void scene(TinyGFX& g, void* ctx) {   // 帯の数だけ呼ばれる
  g.fillCircle(120, 120, 40, TFT_CYAN);      // 座標は画面全体のもの
}
canvas.render(scene);
```

必要な RAM は**幅 × 行数 × 2 バイト**だけ（240px × 1 行 = 480 バイト）。
**行数を変えても絵は 1 画素も変わりません** — テストがそれを検査しています。

## フォント

**ライブラリはフォントデータを 1 バイトも同梱していません。** フォントはスケッチ側に置きます。

### 作りかた

[LGFXFontToolJs](https://www.npmjs.com/package/lgfx-font-tool) の CLI が
**プロジェクトで使う文字だけ**を焼き込める形で出します。

```sh
npx lgfx-font build --google "Noto Sans JP" --em 12 \
    --chars "温度設定完了 23.5℃" --format cellfont --out font.h
```

> `node_modules` のあるディレクトリで実行すると npx が 404 を返します。
> そのときは `npx -p lgfx-font-tool lgfx-font ...` と書いてください。

上の 12 文字なら **245 バイト**。使う文字を増やした分だけ増えます。

出力は**純粋な CellFont** で、TinyGFX の名前を 1 つも含みません。
`setFont()` に渡すには 1 行包みます。

```cpp
#include <TinyGFX.h>
#include <TinyGFX/FontCell.h>   // デコーダ。使う形式だけ
#include "font.h"               // CLI が出したもの。手を入れない

static const TinyGFXFontRef myFont = {&font, &tinygfxFontCellOps, nullptr};

lcd.setFont(&myFont);
lcd.drawString("23.5", 8, 8);
```

`TinyGFX.h` が CellFont の型を連れてくるので、**include の順序は気にしなくて構いません**
（型だけなので 0 バイトです）。

### 形式は 2 つ

**使わないほうのデコーダはリンクされません。**

| 形式 | 向き |
| --- | --- |
| **CellFont**（`FontCell.h`） | **高さ 16 画素以下**、または**字数が少ない**もの |
| u8g2（`FontU8g2.h`） | 16 画素超**かつ字数が多い**帯。RLE と bbox が効きます |

**CellFont の仕様は TinyGFX の外にあります** —
[LGFXFontToolJs](https://github.com/tanakamasayuki/LGFXFontToolJs) の
`docs/formats/cellfont.ja.md`。TinyGFX はその描画器の 1 実装です。
デコーダの大きさは 2 形式でほぼ同じ（684 B / 693 B）なので、選択はデータ量で決まります。

連鎖（`next`）は形式をまたげるので、**半角を CellFont、全角を u8g2**という組み方もできます。

## 入れかた

Arduino IDE のライブラリマネージャからはまだ入りません（未リリース）。
いまは ZIP かクローンで `libraries/` に置いてください。

```cpp
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>     // 使うバス
#include <TinyGFX/PanelST7789.h>    // 使うパネル
```

## ドキュメント

設計の記録は日本語のみです。[docs/README.ja.md](docs/README.ja.md) が案内。

| 読みたいこと | 文書 |
| --- | --- |
| 何を作るライブラリで、どこまでが責務か | [docs/REQUIREMENTS.ja.md](docs/REQUIREMENTS.ja.md) |
| API の形と内部構造 | [docs/CORE_DESIGN.ja.md](docs/CORE_DESIGN.ja.md) |
| **なぜそう設計したのか（理由と、採らなかった案）** | [docs/DECISIONS.ja.md](docs/DECISIONS.ja.md) |
| フラッシュ・RAM の予算と実測 | [docs/FOOTPRINT.ja.md](docs/FOOTPRINT.ja.md) |
| フォントまわりの実測（形式そのものは外部仕様） | [docs/FONT_FORMAT.ja.md](docs/FONT_FORMAT.ja.md) |
| テストの方針 | [docs/TEST_PLAN.ja.md](docs/TEST_PLAN.ja.md) |
| **実機で何を確かめるか** | [docs/MANUAL_TEST.ja.md](docs/MANUAL_TEST.ja.md) |
| 現在地と残作業 | [docs/DEVELOPMENT_PLAN.ja.md](docs/DEVELOPMENT_PLAN.ja.md) |

## テスト

```sh
cd tests && uv sync && uv run pytest -v -s
```

実機は要りません。ホスト実行と、ビルドしてサイズ・シンボルを見るだけのテストです。
詳細は [tests/README.ja.md](tests/README.ja.md)。

45 本のうち特徴的なもの:

- **`linkprune/`** — 使っていない機能・フォント形式が最終バイナリに残っていないことを `nm` で検査
- **`footprint/`** — 構成ごとの増分が予算内か。**数字は常に出します**
- **`tile/`** — 帯の行数を変えても直接描画と 1 画素も違わないこと
- **`hostbus/`** — 本番の SPI バスが実際に流したバイトを拾って画に戻す
- **`i2c/`** — 同じことを I2C + SSD1306 で
- **`hw/m5stack/`** — **実機（Tier 3）。** M5Stack の上で描いた絵をホストのゴールデンと突き合わせる
- **`clifont/`** — **本番の生成器（LGFXFontToolJs CLI）が出した CellFont** をそのまま描けること
- **`fillchunk/`** — まとめ書きを有効にしても線に出るバイトが 1 つも変わらないこと
- **`fontchain/`** — 前段のフォントに豆腐があっても後段の字が出ること（**わざと壊して落ちることを確認済み**）
- **`ili9342/`** — MADCTL・色順・ミラーの組み立て（**表が実機で正しいかは M0 で確かめます**）

## ライセンス

MIT。[LICENSE](LICENSE) を見てください。
