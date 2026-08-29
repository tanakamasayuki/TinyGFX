# API リファレンス

> English: [API.md](API.md)

`TinyGFX` の公開 API と、**それぞれの値札**。

使い方の説明は [../README.ja.md](../README.ja.md)、設計の理由は
[DECISIONS.ja.md](DECISIONS.ja.md)、フットプリントの全体像は
[FOOTPRINT.ja.md](FOOTPRINT.ja.md) にある。

## 値札

**呼ばなかった API は 1 バイトも載らない。** ヘッダオンリーで、描画コアに
仮想関数が 1 つも無いので、リンカではなくコンパイラの段階で消える
（[DECISIONS.ja.md](DECISIONS.ja.md) D1）。だから「使うぶんだけ」の表が
そのまま意味を持つ。

基準機 CH32V003（`-Os`）で実測。**パネル・バス・`fillRect` まで載った状態
（7,772 B）からの増分**。

| API | 増分 | |
| --- | ---: | --- |
| `fillScreen` / `clear` | **48** | `fillRect` の別名 |
| `drawPixel` | **56** | 1x1 の `fillRect` |
| `drawFastHLine` / `drawFastVLine` | **60** | 同上 |
| `setRotation` | **72** | パネルの MADCTL を叩くだけ |
| `setClipRect` / `resetClipRect` | **152** | |
| `drawRect` | 284 | |
| `fillCircle` | 388 | |
| `pushImage` | 392 | |
| `drawLine` | 436 | Bresenham |
| `drawCircle` | 472 | |
| `drawTriangle` | 520 | |
| `pushImage`（透過版） | 608 | |
| `fillRoundRect` | 612 | |
| `fillTriangle` | **832** | 走査線の並べ替えが要る |
| `drawRoundRect` | **868** | **`fillRoundRect` より重い。** 弧を 4 つ描くため |
| 文字（`drawChar` / `drawString` / `textWidth`） | **1,028〜1,132** | 下記 |
| `TinyGFXPrint` + `print(long)` | +280 | 文字の上に |
| `print(float)` | **載らない** | 下記 |

### 表の読み方

**足し算にならない。** どれも `fillRect` の上に乗っていて、互いにコードを
共有する。`drawCircle` と `fillCircle` の両方を使っても 472 + 388 にはならず、
`drawString` は `drawChar`（1,028）のわずか +92 で済む。

**重いものから 2 つ**だけ覚えておけばいい。

- **`drawRoundRect` が `fillRoundRect` より重い**（868 対 612）。塗りは走査線で
  済むが、枠は弧を 4 つ別々に描くため。角丸の枠が要るだけなら、塗りで下地を
  作って内側を背景色で塗るほうが小さいことがある
- **文字は 1,028 B から。** 内訳のほとんどは CellFont のデコーダで、
  `textWidth` を呼ぶだけでも 1,044 B かかる（索引と連鎖の走査が要るため）。
  **1 文字でも描くなら全部載る**ので、そこから先は誤差

### 載らないもの

**`print(float)` / `println(float)` は「重い」ではなく「載らない」。**
CH32V003 で **FLASH が 2,724 バイト溢れる**（実測）。AVR や ESP32 では載るが、
基準機で通らないものを API として勧めることはしない。

整数を出したいだけなら `print(long)` が +280 B で済む。小数が要るなら
自分で整数に直して桁を入れる（`v / 100` と `v % 100`）ほうが、どの環境でも
確実に小さい。

## 契約

### `begin()` が返すもの

```cpp
bool begin();
```

**返り値は「設定が使えるか」であって「パネルが居るか」ではない。**
TinyGFX が相手にするパネルは通常運用では書き込み専用で、4 線 SPI には
応答そのものが無い。`true` を「画面が生きている」と読むと間違える。

`false` になるのはコンパイラに見えない種類の誤りだけ — フレームバッファが
`nullptr`、高さがページの整数倍でない、寸法が 0。**このときは 1 バイトも
送らない。**

### 座標は画面の外でもいい

すべての描画 API は**画面外の座標を受け取ってクリップする。** 負の座標も、
`int16_t` の端の座標も、極端に大きい幅も同じ。

```cpp
lcd.fillRect(-100, -100, 32767, 32767, TFT_RED);  // 画面ぶんだけ塗られる
```

クリップの矩形が空なら 1 バイトも送らない。

### `startWrite()` / `endWrite()`

```cpp
void startWrite();   // 入れ子にできる
void endWrite();
```

バスのトランザクションを開いて閉じる。**入れ子を数えている**ので、
`startWrite()` を 2 回呼んだら `endWrite()` も 2 回要る。

個々の描画 API は自分で開閉するので、**普通は呼ばなくていい。**
連続して何十回も描くときに、外側で 1 回開いておくと転送のたびの
開閉が減る。

### バスは呼び出し側のもの

**TinyGFX は `SPI.begin()` も `Wire.begin()` も呼ばない。** ピンも選ばない。

```cpp
SPI.begin();          // スケッチが持つ
lcd.begin();
```

同じ線に SD カードを繋いでも壊れないようにするため。`beginTransaction` /
`endTransaction` は Arduino の作法どおりに包んである。

### フレームバッファは呼び出し側のもの

ページ方式のモノクロパネル（SSD1306 / SH1106）は**フレームバッファを
自分で確保しない。**

```cpp
static uint8_t fb[128 * 64 / 8];              // 1,024 B
TinyGFXPanelSSD1306 panel(bus, fb, 128, 64);
```

RAM が足りなければ**帯**で描ける（1 ページ 128 B）。
[FOOTPRINT.ja.md](FOOTPRINT.ja.md) §6.4 に数字がある。

## 一覧

### 基本

| | |
| --- | --- |
| `TinyGFX(TinyGFXPanel& panel)` | |
| `bool begin()` | 上記の契約 |
| `int16_t width()` / `height()` | **回転を反映した**大きさ |
| `void setRotation(uint8_t r)` / `uint8_t getRotation()` | 0..3。1 と 3 で幅高さが入れ替わる |
| `static constexpr uint16_t color565(r, g, b)` | コンパイル時に畳まれる |

### 図形

すべて `uint16_t color` を最後に取る。座標は `int16_t`。

| | |
| --- | --- |
| `drawPixel(x, y, c)` | |
| `fillRect(x, y, w, h, c)` / `fillScreen(c)` / `clear(c = 0)` | **これが芯。** 他のほぼ全部がここを通る |
| `drawFastHLine(x, y, w, c)` / `drawFastVLine(x, y, h, c)` | |
| `drawRect` / `drawLine` | |
| `drawCircle` / `fillCircle` (cx, cy, r, c) | |
| `drawRoundRect` / `fillRoundRect` (x, y, w, h, r, c) | 値札の注意を参照 |
| `drawTriangle` / `fillTriangle` (x0..y2, c) | |

### クリップ

| | |
| --- | --- |
| `setClipRect(x, y, w, h)` | |
| `resetClipRect()` / `clearClipRect()` | 同じもの。`clearClipRect` は別名 |

### 画像

| | |
| --- | --- |
| `pushImage(x, y, w, h, const uint16_t* data)` | RGB565、行優先 |
| `pushImage(x, y, w, h, data, uint16_t transparent)` | その色は飛ばす |

**データは RAM に置くこと。** AVR の PROGMEM 画像には未対応
（[DECISIONS.ja.md](DECISIONS.ja.md) Q6）。

### 文字

`#include <TinyGFX/FontCell.h>` が要る。**書かなければデコーダは載らない**
（`tinygfxFontCellOps` が未定義でコンパイルが通らない）。

紛らわしいのは、**似た名前のヘッダが自動で入っている**こと。

| ヘッダ | 中身 | `TinyGFX.h` が入れる |
| --- | --- | --- |
| `TinyGFX/CellFont.h` | **型とマクロだけ**（`CellFont` 構造体、`CELLFONT_READ_*`） | **はい。実測 0 バイト** |
| `TinyGFX/Font.h` | `TinyGFXFontRef` / `TinyGFXFontOps` | **はい**（`Gfx.h` 経由） |
| `TinyGFX/FontCell.h` | **デコーダ本体**（約 1,000 B） | **いいえ** |

`CellFont.h` が既定で入っているのは、生成されたフォントヘッダが**自分では
何も include せず**、型が無ければ `#error` で止まる仕様だから（CellFont
仕様 §12.2）。これが無いと「フォントを `TinyGFX.h` より前に include すると
壊れる」という順序の罠ができる。型とマクロしかないので値札は 0。

| | |
| --- | --- |
| `setFont(const TinyGFXFontRef*)` / `getFont()` | |
| `drawString(const char* s, x, y)` | 戻り値は進んだ幅 |
| `drawCenterString(s, cx, y)` | `cx` を中心に。**+116 B**（呼ばなければ 0） |
| `drawRightString(s, rx, y)` | 右端を `rx` に。**両方使って +232 B** |
| `drawChar(uint16_t ch, x, y)` | |
| `textWidth(const char* s)` | **これだけでも 1,044 B** |
| `setTextColor(fg)` / `setTextColor(fg, bg)` | 2 引数版は背景セルを塗る |
| `setTextSize(uint8_t)` / `getTextSize()` | 整数倍のみ |
| `setCursor(x, y)` / `getCursorX()` / `getCursorY()` | `TinyGFXPrint` 用 |
| `fontHeight()` / `getTextLineHeight()` / `getTextAscent()` | |
| `getTextColor()` / `getTextBgColor()` / `hasTextBg()` | **フォントのデコーダのため。** 描画器は `TinyGFX&` を受け取る自由関数なので、文字の状態をここから読む。普通のスケッチには要らない |

`y` は**行の上端**。ベースラインではない。

収録外の文字は U+FFFD に退避する。フォントが U+FFFD を持っていなければ
何も描かない（豆腐は出ない）。

### 生のウィンドウ

| | |
| --- | --- |
| `setAddrWindow(x, y, w, h)` | |
| `writeColor(uint16_t c, uint32_t count)` | |
| `writePixels(const uint16_t* data, uint32_t count)` | |

パネルに直接流す。**クリップは効かない。** 自分で範囲を保証すること。

## マクロ

すべて既定は「全部入り」。**何も定義しなければ挙動は変わらない。**

| マクロ | 既定 | 0 にすると | CH32V003 |
| --- | --- | --- | ---: |
| `TINYGFX_FONT_BG` | 1 | `setTextColor` の第 2 引数が効かなくなる | −108 |
| `TINYGFX_FONT_SCALE` | 1 | `setTextSize(2)` 以上が効かなくなる | −116 |
| `TINYGFX_FONT_CHAIN` | 1 | `CellFont::next` の連鎖を辿らない | −16 |
| `TINYGFX_FONT_SPARSE` | 1 | 疎索引のフォントが**描けなくなる** | フォント次第 |
| `TINYGFX_FONT_RECORDS` | 1 | 可変ピッチのフォントが**描けなくなる** | フォント次第 |
| `TINYGFX_MONO_FAST_FILL` | 1 | モノクロの塗りが 1 画素ずつになる | −428 |
| `TINYGFX_FILL_CHUNK` | 0 | （1 以上でまとめ書き。速度のみ） | — |

**`SPARSE` と `RECORDS` はフォントと合っていないと違うグリフを描く。**
符号化は実行時のデータなのでコンパイラには見えない。生成ヘッダの
`Format :` 行が唯一の手がかり。

## 入れていないもの

LovyanGFX 系（[LGFXVirtualCanvas](https://github.com/tanakamasayuki/LGFXVirtualCanvas)）
には約 150 の API があり、TinyGFX には 51 しかない。**足りないのではなく、
基準機に載らないものを最初から持たない**という選択。

| 無いもの | 理由 |
| --- | --- |
| `drawFloat` / `print(float)` | **基準機で 2,724 B 溢れる**（実測）。`dtostrf` がそもそも CH32V003 のコアに無い |
| `drawJpg` / `drawPng` / `drawBmp` / `drawQoi` | デコーダだけで基準機の FLASH を使い切る |
| `pushImageRotateZoom` / `setPivot` | 回転拡大は浮動小数か固定小数の補間が要る |
| `drawArc` / `drawEllipse` / `drawBezier` | 三角関数か媒介変数。要望が出たら実測してから |
| `setTextDatum` / `getTextDatum` | **入れない。** 揃えは `drawCenterString` / `drawRightString` で提供する（下記） |
| `drawNumber` | **+168 B で足せる**（実測）。整数から文字列への変換 |
| 1bpp ビットマップの描画 | **+120 B で足せる**（実測）。アイコン用途。モノクロパネルでは `pushImage`（16bpp）より素直 |
| パレット・色深度の切り替え | コアに色深度の抽象を持たない（D4） |
| `readPixel` | パネル側にある。`TinyGFXPanelDcs::readPixels()`。**1 画素 150us** のデバッグ用 |

下 2 つは**測ってあるので、要ると分かったら足せる。** 測る前に足さない。

### `setTextDatum` を入れなかった理由

LovyanGFX は文字の揃えを**2 通り**で提供している — `drawCenterString()` と、
`setTextDatum(TC_DATUM)` + `drawString()`。同じことが 2 系統ある。

TinyGFX は**明示関数だけ**にした。理由は値札で、CH32V003 での実測:

| | 文字は描くが揃えは使わないスケッチ |
| --- | ---: |
| 揃え機能なし | 8,892 |
| `setTextDatum` を持つ | 9,096（**+204**） |
| `drawCenterString` / `drawRightString` を持つ（呼ばない） | 8,892（**+0**） |

**datum は `drawString` が毎回参照する状態**なので、`drawString` が無条件に
`textWidth` を引き込む。中央揃えを一度も使わない人まで払うことになる。
明示関数はインラインのメンバなので、呼ばなければ生成すらされない。
