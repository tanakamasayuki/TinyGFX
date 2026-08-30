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
| `drawBitmap`（1bpp） | 284 | ランを 1 回の `fillRect` にまとめる |
| `drawLine` | 436 | Bresenham |
| `drawCircle` | 472 | |
| `drawTriangle` | 520 | |
| `pushImage`（透過版） | 608 | |
| `fillRoundRect` | 612 | |
| `fillTriangle` | **832** | 走査線の並べ替えが要る |
| `drawRoundRect` | **868** | **`fillRoundRect` より重い。** 弧を 4 つ描くため |
| 文字（`drawChar` / `drawString` / `textWidth`） | **1,192〜1,296** | 下記。**UTF-8 の 164 B 込み** |
| `TinyGFXPrint` + `print(long)` | +404 | 文字の上に。うち 128 B は UTF-8 の状態機械 |
| `print(float)` | **載らない** | 下記 |

### 表の読み方

**足し算にならない。** どれも `fillRect` の上に乗っていて、互いにコードを
共有する。`drawCircle` と `fillCircle` の両方を使っても 472 + 388 にはならず、
`drawString` は `drawChar`（1,028）のわずか +92 で済む。

**重いものから 2 つ**だけ覚えておけばいい。

- **`drawRoundRect` が `fillRoundRect` より重い**（868 対 612）。塗りは走査線で
  済むが、枠は弧を 4 つ別々に描くため。角丸の枠が要るだけなら、塗りで下地を
  作って内側を背景色で塗るほうが小さいことがある
- **文字は 1,192 B から。** 内訳のほとんどは CellFont のデコーダで、
  `textWidth` を呼ぶだけでも 1,044 B かかる（索引と連鎖の走査が要るため）。
  **1 文字でも描くなら全部載る**ので、そこから先は誤差。
  164 B は UTF-8 の復号で、`-DTINYGFX_FONT_UTF8=0` で丸ごと戻る

### 載らないもの

**`print(float)` / `println(float)` は「重い」ではなく「載らない」。**
CH32V003 で **FLASH が 2,724 バイト溢れる**（実測）。AVR や ESP32 では載るが、
基準機で通らないものを API として勧めることはしない。

整数を出したいだけなら `print(long)` が +404 B で済む。小数が要るなら
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
static uint8_t fb[TinyGFXPanelSSD1306_128x64::kBufferBytes];              // 1,024 B
TinyGFXPanelSSD1306_128x64 panel(bus, fb);
```

RAM が足りなければ**帯**で描ける（1 ページ 128 B）。
[FOOTPRINT.ja.md](FOOTPRINT.ja.md) §6.4 に数字がある。

## 一覧

### 基本

| | |
| --- | --- |
| `TinyGFX(TinyGFXTarget& panel)` | |
| `bool begin()` | 上記の契約 |
| `int16_t width()` / `height()` | **回転を反映した**大きさ |
| `void setRotation(uint8_t r)` / `uint8_t getRotation()` | 0..3。1 と 3 で幅高さが入れ替わる |
| `static constexpr uint16_t color565(r, g, b)` | コンパイル時に畳まれる |

#### 色定数

**2 つの綴りがあります。どちらもマクロなので 0 バイトです。**

| 綴り | 例 | 性質 |
| --- | --- | --- |
| `TFT_*` | `TFT_RED` | TFT_eSPI / LovyanGFX と同じ名前。**1 つずつ `#ifndef` で囲ってある**ので、先に定義しているライブラリがあればそちらが勝つ |
| `TINYGFX_*` | `TINYGFX_RED` | 無条件。**奪われない名前**が要るときに（D30） |

16 色（`BLACK` `NAVY` `DARKGREEN` `MAROON` `PURPLE` `OLIVE` `DARKGREY` `BLUE`
`GREEN` `CYAN` `RED` `MAGENTA` `YELLOW` `WHITE` `ORANGE` `PINK`）。
それ以外は `color565(r, g, b)` で作れます（コンパイル時に畳まれます）。

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
| `clipX0()` / `clipY0()` / `clipX1()` / `clipY1()` | **復号器のため。** 両端を含む。自分で窓を開く復号器（`setAddrWindow` は**クリップしない**）が自前でクリップするのに使う。普通の描画には要らない |

### 画像

| | |
| --- | --- |
| `pushImage(x, y, w, h, const uint16_t* data)` | RGB565、行優先 |
| `pushImage(x, y, w, h, data, uint16_t transparent)` | その色は飛ばす |
| `drawBitmap(x, y, const uint8_t* bitmap, w, h, color)` | **1bpp。アイコン用。+284 B** |
| `tinygfx_swapBytes565(uint16_t* data, uint32_t count)` | RGB565 のバイト順をその場で入れ替える。**`setSwapBytes` の代わり。呼ばなければ 0 B** |

`pushImage` のデータは **RAM に置くこと**（[DECISIONS.ja.md](DECISIONS.ja.md) D27）。

**この制限があるのは AVR だけです。** CH32V003 や ESP32 はフラッシュが
アドレス空間に見えているので、`static const uint16_t img[] = {...}` を
そのまま渡せます。AVR でフラッシュに置きたいときは `drawImage`（次節）を
使ってください —— 変換ツールの出す raw565 が同じ生 RGB565 で、
**PROGMEM から読みます。**

`drawBitmap` は逆で、**フォントと同じ読み方（`tinygfx_rd8`）をするので
AVR では PROGMEM に置くこと。** ビットは MSB first、**各行がバイト境界から
始まる**（1 行 `(w + 7) / 8` バイト）。Adafruit_GFX・U8g2・LovyanGFX と同じ
並びなので、アイコン変換ツールの出力がそのまま使える。

```cpp
static const uint8_t icon[] TINYGFX_FONT_PROGMEM = {
  0x18, 0x24, 0x42, 0x81, 0x81, 0x42, 0x24, 0x18,
};
lcd.drawBitmap(10, 10, icon, 8, 8, TFT_WHITE);
```

1 のビットだけを塗り、0 は触らない（透過）。背景も塗りたいなら先に
`fillRect` するほうが、第 2 の色を持ち回るより小さい。

**立っているビットの連なりを 1 回の `fillRect` にまとめる**ので、
`fillRect` を奪ったパネルはこれも面倒を見る。画素ごとに置く実装より
84 B 大きいが、カラーパネルでは窓設定の回数が桁で減る。

### 生成した画像を描く

```cpp
#include <TinyGFX/Image.h>
#include "my_icon.h"          // 変換ツールの出力
lcd.drawImage(&my_iconRef, 10, 10);
```

**形式を利用者が選ぶ必要はない。** 変換ツールが絵ごとに最小の符号化を
総当たりで選び、生成ヘッダが自分のデコーダを指す。**使う形式のデコーダ
だけがリンクされる**ので、`Image.h` が全形式を持っていても未使用は 0 バイト。

| 形式 | CH32V003 | AVR | ESP32 |
| --- | ---: | ---: | ---: |
| 生 RGB565 | 400 | 638 | 492 |
| RLE | 387 | 663 | 467 |
| RLE + 4bit パレット | 395 | 679 | 483 |
| 1bpp（横／縦） | 392〜400 | 654〜690 | 592 |

**画像を何枚も使うならまとめて変換すること。** デコーダ代は形式ごとに
1 回なので、1 枚ずつ最小を選ぶと形式が散らばって損をする（実測で
343〜597 B）。詳細は [IMAGE_FORMAT.ja.md](IMAGE_FORMAT.ja.md)。

ページ方式のモノクロパネルには、ページ境界に揃った縦詰めを直接貼る
`TinyGFXDriverPaged::pushVBitmap()` もある。揃っていなければ `false` を
返すので、`drawImage()` に落とせばいい。

### 生データと生成物 —— 2 つの入口

画像には入口が 2 つあるが、**同じことの 2 通りではない。** 入力の出所が違う。

| | 何を渡すか | ツール |
| --- | --- | --- |
| **生データ** `pushImage` / `drawBitmap` | 配列を直に。寸法も色も呼び手が指定 | **要らない** |
| **生成物** `drawImage` | 自己記述の構造体。符号化はツールが選ぶ | 要る |

ネット上のアイコン変換ツールは軒並み Adafruit 形式の 1bpp 配列を吐くので、
**`drawBitmap` はそれをビルド手順なしでそのまま貼れる。** 実測でも単純な
用途では安い:

| | CH32V003 | AVR |
| --- | ---: | ---: |
| `drawBitmap` のみ | **308** | **368** |
| `drawImage`（1bpp）のみ | 556 | 832 |
| 両方 | 808 | 1,038（**重複 −162**） |

併用しても重複は少ない（ランのまとめと `fillRect` の経路を共有する）。
**どちらも呼ばなければ 0 バイト。**

### オフスクリーンに描く（スプライト相当）

**専用の API は無い。** `TinyGFXMemoryTarget` が RAM バッファをパネルとして
見せるので、そこに `TinyGFX` を建てて描き、`pushImage()` で画面に戻す。

```cpp
static uint16_t sprBuf[16 * 16];              // 512 B
TinyGFXMemoryTarget sprPanel(sprBuf, 16, 16);
TinyGFX spr(sprPanel);

spr.begin();
sprPanel.fillBuffer(TFT_BLACK);
spr.fillCircle(8, 8, 6, TFT_RED);             // 普通の描画 API が全部使える

lcd.pushImage(10, 10, 16, 16, sprBuf);                  // そのまま貼る
lcd.pushImage(10, 50, 16, 16, sprBuf, TFT_BLACK);       // 黒を透過して貼る
```

`TinyGFXMemoryTarget` には `readPixel(x, y)` と `fillBuffer(color)` もある。

**1 画素 2 バイトなので大きさに注意。** 基準機 CH32V003（RAM 2KB）では
16x16 が 512 B で、**32x32 は 2,048 B になって載らない**（実測）。
画面全体を持ちたいだけなら、スプライトではなく
[`TinyGFX/TileCanvas.h`](../src/TinyGFX/TileCanvas.h) の帯描画を使う。

### 帯で描く（ちらつき対策）

`#include <TinyGFX/TileCanvas.h>`。**+944 B / RAM は帯のぶんだけ。**

| | |
| --- | --- |
| `TinyGFXTileCanvas(TinyGFXTarget& target, uint16_t* buffer, uint32_t bufferPixels)` | バッファは呼び出し側のもの |
| `bool begin()` / `setRotation(r)` | |
| `TinyGFX& gfx()` | フォントや色をここで設定する。`render()` をまたいで残る |
| `render(callable)` | **任意の callable**（ラムダなど）。引数は `TinyGFX&` 1 つ |
| `render(void (*fn)(TinyGFX&, void*), void* ctx = nullptr)` | 関数ポインタ版。**同じ大きさ**（D28） |
| `setBackgroundColor(c)` / `setAutoClear(bool)` | |
| `int16_t tileRows()` | 1 帯の行数。バッファが 1 行に足りなければ 0 |

**描画関数は帯の数だけ呼ばれる。** 座標は画面全体のもので、
オフセットとクリップはこちらで面倒を見る。

**行数を変えても絵は 1 画素も変わらない**（`tests/tile/` が検査）。
速度と RAM の交換にしかならない。

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
| `drawChar(uint16_t ch, x, y)` | 引数は**コードポイント**（バイトではない） |
| `textWidth(const char* s)` | **これだけでも 1,044 B** |
| `TinyGFX::nextCode(const char*& p)` | 静的。1 文字進めてコードポイントを返す。**呼ばなければ 0 B** |
| `setTextColor(fg)` / `setTextColor(fg, bg)` | 2 引数版は背景セルを塗る |
| `setTextSize(uint8_t)` / `getTextSize()` | 整数倍のみ |
| `setCursor(x, y)` / `getCursorX()` / `getCursorY()` | `TinyGFXPrint` 用 |
| `fontHeight()` / `getTextLineHeight()` / `getTextAscent()` | |
| `getTextColor()` / `getTextBgColor()` / `hasTextBg()` | **フォントのデコーダのため。** 描画器は `TinyGFX&` を受け取る自由関数なので、文字の状態をここから読む。普通のスケッチには要らない |

`y` は**行の上端**。ベースラインではない。

収録外の文字は U+FFFD に退避する。フォントが U+FFFD を持っていなければ
何も描かない（豆腐は出ない）。

#### 文字列は UTF-8

`drawString` / `textWidth` / `TinyGFXPrint` の `print` は **`char*` を UTF-8 として
読む**。誰かが選んだわけではなく、**そう保存されているから**である。Arduino IDE は
スケッチを UTF-8 で保存するので、`"25°C"` はソースの時点で 4 バイトではなく
5 バイトになっている。バイトを 1 文字として読むと、度記号 1 つが 2 文字、
仮名 1 つが 3 文字になり、しかも**黙って**そうなる。

| 入力 | 結果 |
| --- | --- |
| U+0000〜U+FFFF | そのコードポイント |
| U+FFFF 超（絵文字など） | **U+FFFD。ただし 4 バイトすべて消費する**（後ろがずれない） |
| 途中で切れた列 | U+FFFD。**壊れたバイトの手前で止まる**（終端を飛び越えない） |
| 先導のない継続バイト・0xFE・0xFF | U+FFFD。1 バイトだけ消費 |
| 冗長符号化（overlong） | **拒否せず復号する。** 指しているのは元々描ける文字なので、ここで弾いても得るものがない |

U+FFFF を超えられないのは、フォント側の入口が `uint16_t` だから
（`TinyGFXFontOps::draw`）。

`nextCode` を公開しているのは、**折り返しや部分幅を自分で計算するコードが
同じ歩き方をしないと答えが合わない**ため。

```cpp
const char* p = s;
while (*p) {
  const char* start = p;
  uint16_t cp = TinyGFX::nextCode(p);   // p が 1 文字進む
  ...
}
```

`TINYGFX_FONT_UTF8` を 0 にするとバイト単位（Latin-1）に戻る。**バイト単位で
測ったサイズに 1 バイトの差もなく戻る**（実測）。

### 生のウィンドウ

| | |
| --- | --- |
| `setAddrWindow(x, y, w, h)` | |
| `writeColor(uint16_t c, uint32_t count)` | |
| `writePixels(const uint16_t* data, uint32_t count)` | |

パネルに直接流す。**クリップは効かない。** 自分で範囲を保証すること。

## マクロ

**何も定義しなければ挙動は変わらない。** 大半は既定 1 の切り落としで、
`TINYGFX_TEXT_WRAP` と `TINYGFX_FILL_CHUNK` だけが既定 0 の後付け。

| マクロ | 既定 | 0 にすると | CH32V003 |
| --- | --- | --- | ---: |
| `TINYGFX_FONT_BG` | 1 | `setTextColor` の第 2 引数が効かなくなる | −104 |
| `TINYGFX_FONT_SCALE` | 1 | `setTextSize(2)` 以上が効かなくなる | −100 |
| `TINYGFX_FONT_CHAIN` | 1 | `CellFont::next` の連鎖を辿らない | −8 |
| `TINYGFX_FONT_UTF8` | 1 | 文字列をバイト単位（Latin-1）で読む | **−148**（`TinyGFXPrint` も使っていれば −292） |
| `TINYGFX_TEXT_WRAP` | **0** | （1 で `TinyGFXPrint::setTextWrap()` が現れる） | **+164** |
| `TINYGFX_FONT_SPARSE` | 1 | 疎索引のフォントが**描けなくなる** | フォント次第 |
| `TINYGFX_FONT_RECORDS` | 1 | 可変ピッチのフォントが**描けなくなる** | フォント次第 |
| `TINYGFX_MONO_FAST_FILL` | 1 | モノクロの塗りが 1 画素ずつになる | −408 |
| `TINYGFX_FILL_CHUNK` | 0 | （1 以上でまとめ書き。速度のみ） | — |

**`SPARSE` と `RECORDS` はフォントと合っていないと違うグリフを描く。**
符号化は実行時のデータなのでコンパイラには見えない。生成ヘッダの
`Format :` 行が唯一の手がかり。

## 入れていないもの

LovyanGFX 系（[LGFXVirtualCanvas](https://github.com/tanakamasayuki/LGFXVirtualCanvas)）
には約 150 の API があり、TinyGFX には 58 しかない。**足りないのではなく、
基準機に載らないものを最初から持たない**という選択。

| 無いもの | 理由 |
| --- | --- |
| `drawFloat` / `print(float)` | **基準機で 2,724 B 溢れる**（実測）。`dtostrf` がそもそも CH32V003 のコアに無い |
| `drawJpg` / `drawPng` / `drawBmp` / `drawQoi` | デコーダだけで基準機の FLASH を使い切る |
| `pushImageRotateZoom` / `setPivot` | 回転拡大は浮動小数か固定小数の補間が要る |
| `drawArc` / `drawEllipse` / `drawBezier` | 三角関数か媒介変数。要望が出たら実測してから |
| `setTextDatum` / `getTextDatum` | **入れない。** 揃えは `drawCenterString` / `drawRightString` で提供する（下記） |
| 糖衣クラス（`TinyGFX_ST7789_SoftSPI` のようなもの） | **入れない。** 大きさは変わらない（実測 ±0）が、**バスを利用者が持っていることがこのライブラリの売り**で、糖衣クラスは中でバスを作ってしまう（D31） |
| `setTextWrap` | **既定は入っていない。** `TINYGFX_TEXT_WRAP=1` で `TinyGFXPrint::setTextWrap()` が現れる。**+164 B** —— 折り返しは「描く前に送り幅を知る」必要があり、フォントデコーダへの 2 本目の入口が要る（D33） |
| `setSwapBytes` | **入れない。** 実行時のモードは「使わないスケッチにも +44 B / RAM +4 B」（画素ごとの分岐）。代わりに `tinygfx_swapBytes565(data, count)` を呼ぶ。**呼ばなければ 0 B、呼べば +32 B**（D29） |
| `drawNumber` | **+168 B で足せる**（実測）。整数から文字列への変換 |
| パレット・色深度の切り替え | コアに色深度の抽象を持たない（D4） |
| `readPixel` | パネル側にある。`TinyGFXDriverDcs::readPixels()`。**1 画素 150us** のデバッグ用 |

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
