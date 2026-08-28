# コア設計

内部の記録。日本語のみ。**API の形と内部構造。**

たたき台。**なぜそうしたか**は [DECISIONS.ja.md](DECISIONS.ja.md) にある。ここには**何がどうなっているか**だけを書く。

## 1. 層構造

```text
アプリケーション
  |
  v
TinyGFX          描画コア（座標・クリップ・プリミティブ・文字）
  |              非 virtual / ヘッダ inline / テンプレートなし
  v
Panel            LCD コントローラ（初期化列・MADCTL・オフセット・ウィンドウ）
  |              ← ここに virtual が 1 枚
  v
Bus              転送（SPI の叩き方・DC / CS）
                 ← ここに virtual が 1 枚
  +-- TinyGFXBusSoftSPI  ビットバン。pinMode/digitalWrite だけ。**第一実装**（D18）
  +-- TinyGFXBusSPI      Arduino SPI。使える環境ではこちらが速い
  +-- TinyGFXBusCapture  テスト用。コマンド列を仮想 GRAM に書き戻す
  +-- （将来）MCU 固有の Fast Backend
```

**virtual は Panel と Bus の 2 枚だけ。描画 API には 1 つも置かない。** 理由は §7。

## 2. ユーザーが書くコード（この形を確定させたい）

### 2.1 標準形

```cpp
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/PanelST7789.h>
#include "myfont.h"                    // フォントはスケッチ側に置く（D17）

TinyGFXBusSoftSPI  bus(/*sck*/5, /*mosi*/6, /*dc*/3, /*cs*/4);
TinyGFXPanelST7789 panel(bus, 240, 240, /*rst*/2);
TinyGFX            lcd(panel);

void setup() {
  lcd.begin();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BLACK);

  lcd.setFont(&myFont);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("Hello CH32V", 8, 8);

  lcd.fillRect(10, 40, 80, 20, TFT_GREEN);
}

void loop() {}
```

3 オブジェクトを並べる形。冗長だが、**どのピンがどの層のものかが見える**のと、
Bus の差し替えが 1 行で済む。ハードウェア SPI が使える環境では
`#include <TinyGFX/BusSPI.h>` にして `TinyGFXBusSPI bus(/*dc*/3, /*cs*/4);` に差し替えるだけ。

`TinyGFXBusSPI` が SCK / MOSI を取らないのは、Arduino の `SPI.begin()` が
ピンを取らないコアがあるため。ピンを指定したい環境では自分で `SPI.begin(...)` を呼び、
`initSpi=false` を渡す。

**暫定（Q1）**: よくある組み合わせをまとめた糖衣クラスを用意するかは未定。

### 2.2 ちらつきを避けたいとき

```cpp
#include <TinyGFX/TileCanvas.h>

static uint16_t band[240 * 2];                       // 幅 240 × 2 行 = 960 B
TinyGFXTileCanvas canvas(panel, band, sizeof(band) / 2);

static void scene(TinyGFX& g, void* ctx) {           // 帯の数だけ呼ばれる
  g.fillRect(10, 10, 40, 40, TFT_RED);               // 座標は画面全体のもの
  g.drawString("12:34", 4, 4);
}

void setup() {
  canvas.begin();
  canvas.gfx().setFont(&myFont);
}
void loop() {
  canvas.render(scene);                              // 1 フレーム
}
```

詳細は §12。

## 3. 型と定数

| 用途 | 型 | 備考 |
| --- | --- | --- |
| 座標・寸法 | `int16_t` | 画面外・負値を許す。クリップで落とす |
| 画素数・転送長 | `uint32_t` | 240x320 = 76,800 なので 16bit では足りない |
| 色 | `uint16_t` | RGB565。型エイリアスは作らない |
| 文字コード | `uint16_t` | 将来の多バイト用に幅を確保するだけ。v0.x は ASCII のみ |

```cpp
static constexpr uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)((r & 0xF8) << 8 | (g & 0xFC) << 3 | b >> 3);
}
```

色定数は `TFT_BLACK` / `TFT_WHITE` / `TFT_RED` … を `#ifndef` ガード付きで定義する。既に定義済みならそちらを尊重する。

**暫定**: `TFT_` プレフィクスを使うか、`TINYGFX_` にするか。他ライブラリとの同居を考えると `#ifndef` ガードで十分だと思うが要確認。

## 4. 公開 API

すべて `TinyGFX` のメンバ。**virtual なし。全部ヘッダ内 inline。**

### 4.1 基本

```cpp
bool     begin();
int16_t  width()  const;
int16_t  height() const;
uint8_t  getRotation() const;
void     setRotation(uint8_t r);        // 0..3
void     invertDisplay(bool invert);
void     sleep();
void     wakeup();
```

### 4.2 転送制御

```cpp
void startWrite();   // CS Low + トランザクション開始。ネスト可（カウンタ）
void endWrite();     // カウンタが 0 になったら CS High
```

各描画 API は内部で `startWrite` / `endWrite` を呼ぶ。連続描画するときは外側で囲めば CS のトグルが減る。**LovyanGFX と同じ挙動。**

### 4.3 プリミティブ

```cpp
void drawPixel(int16_t x, int16_t y, uint16_t color);
void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void fillScreen(uint16_t color);
void clear(uint16_t color = 0);          // fillScreen の別名（LovyanGFX 互換）

void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
void drawTriangle(int16_t x0,int16_t y0,int16_t x1,int16_t y1,int16_t x2,int16_t y2, uint16_t color);
void fillTriangle(int16_t x0,int16_t y0,int16_t x1,int16_t y1,int16_t x2,int16_t y2, uint16_t color);
```

すべて整数演算のみ。円は中点円アルゴリズム、線は Bresenham。**除算・剰余を使わない。**

### 4.4 画像・低レベル転送

```cpp
void setAddrWindow(int16_t x, int16_t y, int16_t w, int16_t h);
void writeColor(uint16_t color, uint32_t count);        // 同色を count 回
void writePixels(const uint16_t* data, uint32_t count); // 連続画素
void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data);
void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data, uint16_t transparent);
```

`pushImage` の画素は **ホスト側のバイト順の `uint16_t` 配列**として受け取る。バイトスワップは Bus が担当する（§6）。

**暫定**: PROGMEM（AVR）対応をどうするか。AVR を努力目標に留めるなら `pushImage` は AVR で使えない、で割り切る案がある。

### 4.5 クリップ

```cpp
void setClipRect(int16_t x, int16_t y, int16_t w, int16_t h);
void clearClipRect();
```

クリップ矩形は既定で画面全体。**画面外への描画は必ずここで落とす**ので、各プリミティブに画面外判定を書かない。

### 4.6 文字

```cpp
void    setFont(const TinyGFXFont* font);
void    setCursor(int16_t x, int16_t y);
void    setTextColor(uint16_t fg);
void    setTextColor(uint16_t fg, uint16_t bg);
void    setTextSize(uint8_t size);              // 整数倍のみ。1..8
int16_t drawChar(uint16_t ch, int16_t x, int16_t y);
int16_t drawString(const char* str, int16_t x, int16_t y);
int16_t textWidth(const char* str) const;
int16_t fontHeight() const;
```

- 戻り値は**描いた幅**（LovyanGFX と同じ）。
- **`print` / `println` / `printf` はコアに無い。** 別ヘッダで提供する（§9.3）。
- コアの `setTextSize` は整数倍のみ。**float 版のオーバーロードは拡張ヘッダ側**に置く。

## 5. Panel インターフェース

```cpp
class TinyGFXPanel {
public:
  virtual bool init() = 0;
  virtual void setRotation(uint8_t r) = 0;
  virtual void setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) = 0;
  virtual void writeColor(uint16_t color, uint32_t count) = 0;
  virtual void writePixels(const uint16_t* data, uint32_t count) = 0;
  virtual void beginTransaction() = 0;
  virtual void endTransaction() = 0;

  int16_t width() const;    // 回転後
  int16_t height() const;
};
```

**仮想メソッドは 7 本**。`invertDisplay` / `setSleep` / `displayOn` は
**具象パネル側の非仮想メソッド**にした（Q7 決着）。全員が払うほどのものではないため。

Panel が持つ状態:

| 状態 | 用途 |
| --- | --- |
| パネル固有の解像度 | 240x240 / 240x320 など |
| 原点オフセット | ST7789 の 240x240 モジュールなど、GRAM 原点がずれる個体向け |
| MADCTL 値 | 回転 0..3 に対応する 4 通り |
| 現在の回転 | `width()` / `height()` の入れ替え |

**回転はコントローラの MADCTL でやる。** ソフトで座標変換しない（フラッシュも時間も食うため）。

実装は `TinyGFXPanelST7789` / `TinyGFXPanelILI9342`（カラー・実機）、
`TinyGFXPanelSSD1306`（モノクロ・実機。D21）、
`TinyGFXPanelMemory`（RAM バッファ。テストと帯レンダリング）。

## 6. Bus インターフェース

```cpp
class TinyGFXBus {
public:
  virtual void init() = 0;
  virtual void beginTransaction() = 0;                        // CS Low
  virtual void endTransaction() = 0;                          // CS High
  virtual void writeCommand(uint8_t cmd) = 0;                 // DC Low
  virtual void writeData(const uint8_t* data, size_t len) = 0;// DC High
  virtual void writeColor(uint16_t color, uint32_t count) = 0;
  virtual void writePixels(const uint16_t* data, uint32_t count) = 0;
};
```

責務の所在:

| ピン / 事項 | 所有者 |
| --- | --- |
| SCK / MOSI | Bus |
| **CS** | Bus（`beginTransaction` / `endTransaction`） |
| **DC** | Bus（`writeCommand` / `writeData` の切り替え） |
| **RST** | **Panel**（リセット波形が初期化列とセットのため） |
| バイト順（MSB first への変換） | **Bus** |

`writeColor(color, count)` を**インターフェースに入れているのが要点**。これがないと `fillScreen` が画素分の配列を要求してしまい、RAM 2KB の機種で成立しない。

## 7. 結合方式とフットプリント（この設計の核心）

### 7.1 描画 API に virtual を置かない理由

`-ffunction-sections -Wl,--gc-sections` は**呼ばれない関数を落とす**が、**vtable から参照されている関数は落とせない**。描画メソッドを virtual にすると、`fillRect` しか使わないスケッチでも `fillTriangle` も `drawCircle` もフラッシュに載る。

そこで:

- **描画メソッドはすべて非 virtual・ヘッダ内 inline。** 使わなければ 1 バイトも載らない（[REQUIREMENTS.ja.md](REQUIREMENTS.ja.md) §8 の「構成 A〜E」がそのまま実現できる）。
- **virtual は Panel と Bus の 2 枚だけ。** どちらもメソッド数が少なく、しかも全部使われる。

### 7.2 なぜテンプレートにしないか

`TinyGFX<PanelST7789<BusSPI>>` の形なら間接呼び出しが消えて最速・最小になる可能性はある。採らない理由:

- **サイズが読めなくなる。** inline 展開が効きすぎると、呼び出し箇所ごとに転送ループが複製されてかえって増える。構成 A〜E の積み上げ管理がやりにくい。
- ユーザーが書く型名とコンパイルエラーが重くなる。
- 2 枚の間接呼び出しは**速度**のコストであって**サイズ**のコストではない。速度は多少遅くてよい（[REQUIREMENTS.ja.md](REQUIREMENTS.ja.md) §3）。

### 7.3 逃げ道: 静的結合スイッチ

それでも V003 で足りなければ、**ビルド時に実装を 1 つに固定して virtual を消す**オプションを用意する。

```cpp
#define TINYGFX_STATIC_BUS   TinyGFXBusSPI
#define TINYGFX_STATIC_PANEL TinyGFXPanelST7789
```

定義されると `TinyGFXBus*` の代わりに具象型の参照を持ち、間接呼び出しが直接呼び出しになる。**1 スケッチ 1 パネルの制約と引き換え**。API は変わらない。

**これは Phase 0 で実測してから入れるか決める。** 数字を見ずに複雑さを足さない。

### 7.4 「まわりまわって載る」を防ぐ規則

**未使用機能はリンカが落としてくれればそれでよい。** 問題は、落とせるはずのものが**参照の連鎖でつながって落ちなくなる**こと。次を規則にする。

| # | 規則 | 破るとどうなるか |
| --- | --- | --- |
| R1 | **描画メソッドを virtual にしない** | vtable が全メソッドを参照し、1 つ使うと全部載る（D1） |
| R2 | **関数ポインタのテーブルを持たない** | vtable と同じ。パネル一覧・コマンド表を関数ポインタで持つと全部載る |
| R3 | **コアから拡張ヘッダを参照しない。参照は拡張 → コアの一方向だけ** | `Print` やフォントがコア側から辿れると、全員が払う |
| R4 | **コアは `Serial` を一切参照しない**（デバッグ出力・アサート含む） | 1 行の `Serial.print` で `Print` 一式、書式によっては浮動小数点まで載る |
| R5 | **コンストラクタと `begin()` で「全機能の初期化」をしない** | 既定フォントを設定するだけでフォントデータが載る（D12） |
| R6 | **パネル自動検出を持たない**（使うパネルはユーザーが型で指定する） | 全パネルの初期化テーブルが載る |
| R7 | **共通ヘルパを薄く保つ。重い依存を持つヘルパを作らない** | 軽い機能がヘルパ経由で重いものを引く |
| R8 | **`switch` / テーブルで全機能を列挙する入口を作らない** | 1 つの入口から全部が辿れる |
| R9 | **関数内 `static` を使わない** | ガード変数と初期化コードが載り、落ちなくなる |

規則は**テストで守る**。構成ごとに「載っていてはいけないシンボル」を最終バイナリで検査する（[FOOTPRINT.ja.md](FOOTPRINT.ja.md) §8、[TEST_PLAN.ja.md](TEST_PLAN.ja.md) Tier 0 `linkprune`）。**「たぶん落ちているはず」で済ませない。**

前提とするビルドオプションは `-ffunction-sections -fdata-sections -Wl,--gc-sections`。**要確認（Phase 0）**: 対象コアが既定でこれを付けているか。付いていなければ、付いていない環境での数字も併記する。

## 8. メモリモデル

### 8.1 バッファを持たない

`fillScreen(240x320)` = 76,800 画素。既定実装は **1 画素ずつ 16bit 転送**を 76,800 回まわす。バッファ 0 バイト。遅いが載る。

```cpp
void TinyGFXBusSPI::writeColor(uint16_t color, uint32_t count) {
  while (count--) SPI.transfer16(color);
}
```

### 8.2 速度が要るときだけバッファを使う

```cpp
#define TINYGFX_FILL_CHUNK 32   // 画素数。既定 0 = バッファなし
```

定義すると `writeColor` が **スタック上の一時配列**（32 画素 = 64 バイト）を作ってまとめて転送する。**静的 RAM は増やさない。** ESP32 など RAM に余裕がある機種で使う。

### 8.3 インスタンスの RAM

| オブジェクト | 想定サイズ |
| --- | --- |
| `TinyGFX` | カーソル・色・テキスト設定・クリップ矩形・Panel ポインタ = **約 24 B** |
| `TinyGFXPanel*` 具象 | 解像度・オフセット・回転・RST ピン・Bus ポインタ・vptr = **約 16 B** |
| `TinyGFXBusSPI` | ピン 4 本・vptr = **約 12 B** |

合計 **50 B 前後**。目標は [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md) §8 の 40 B なので、**現状の見積もりでは超えている。** クリップ矩形やテキスト設定を削るか、目標を直すかは Phase 1 で決める。

## 9. 文字とフォント

### 9.1 コアはフォント形式を 1 つも知らない

**形式ごとのデコーダはフォント側が指す。** コアがするのは連鎖をたどって呼ぶことだけ。

```cpp
struct TinyGFXFontOps {          // 形式ごとの入口。収録外は -1 で統一
  int16_t (*draw)(TinyGFX&, const void* font, uint16_t ch, int16_t x, int16_t y);
  int16_t (*advance)(const void* font, uint16_t ch);
  uint8_t (*lineHeight)(const void* font);
};

struct TinyGFXFontRef {          // setFont に渡すもの
  const void* data;
  const TinyGFXFontOps* ops;
  const TinyGFXFontRef* next;    // 持っていない字を探しに行く先。形式が違ってもよい
};
```

コア側はこれだけ:

```cpp
int16_t drawChar(uint16_t ch, int16_t x, int16_t y) {
  for (const TinyGFXFontRef* f = _font; f != nullptr; f = f->next) {
    const int16_t a = f->ops->draw(*this, f->data, ch, x, y);
    if (a >= 0) return (int16_t)(a * _textSize);
  }
  return 0;
}
```

**include していない形式のデコーダはどこからも参照されないので落ちる。**
形式を増やしてもフットプリントは増えない。`tests/linkprune/` がこれを検査している。

> §7.4 の R2 は「関数ポインタの表を持つな」だが、ここは逆の働きをする。
> R2 が禁じているのは**全機能を並べた表**（1 つ使うと全部載る）。ここの表は
> **形式ごとに 1 枚**で、その形式のフォントを include した人だけが参照する。

### 9.2 使い方

```cpp
#include <TinyGFX/FontTiny.h>   // 使う形式だけ include する
#include "myfont.h"             // 生成されたフォント（中で FontRef を定義している）

lcd.setFont(&myFont);
lcd.drawString("12:34", 4, 4);
```

生成されたヘッダはこうなっている:

```cpp
static const TinyGFXFontTiny myFontData = { ... };
static const TinyGFXFontRef  myFont = { &myFontData, &tinygfxFontTinyOps, nullptr };
```

### 9.3 いま用意している形式

| ヘッダ | 形式 | 向き |
| --- | --- | --- |
| `TinyGFX/FontTiny.h` | **TinyFont** | **高さ 16 画素以下**のピクセルグリッドフォント |
| `TinyGFX/FontU8g2.h` | u8g2 | 16 画素超。RLE と bbox が効き始める帯 |

境界が H≈16 なのは実測上の変曲点だから（[FONT_FORMAT.ja.md](FONT_FORMAT.ja.md) §0）。
**デコーダの大きさはほぼ同じ**（TinyFont 828 B / u8g2 1,105 B）なので、
選択はデータ量だけで決まる。

### 9.4 連鎖は形式をまたげる

`next` で繋いだ先は別形式でよい。半角を TinyFont、全角を u8g2、も書ける。

### 9.5 座標はベースラインでなく行の上端

`drawChar(ch, x, y)` の `y` は**行の上端**（LovyanGFX 流）。u8g2 形式のデコーダは
内部で `ascent` を足してベースラインに直している。**形式が違っても同じ位置に出る。**

### 9.6 フォントデータはライブラリに同梱しない

**`src/` に .h のフォントは 1 つも置かない。** スケッチ側に置く。
生成は [LGFXFontToolJs](https://www.npmjs.com/package/lgfx-font-tool) を使い、
最終的には**プロジェクトで使う文字だけ**をサブセット化して埋め込む。

いまは `tools/gen_font.py` が出す 5x7（0x20-0x3F、32 字）が測定用のつなぎ。

### 9.7 Print / printf / float は拡張ヘッダで提供する

**禁止はしない。分けるだけ。** コアの `TinyGFX` は `Print` を継承せず、浮動小数点も出さない。

```cpp
#include <TinyGFX/Print.h>
TinyGFXPrint lcd(panel);
lcd.println(3.14f);           // ここまで来ると浮動小数点書式化がリンクされる
```

**実測した値札**（CH32V003）:

| 取り込むもの | 増分 |
| --- | --- |
| `+ TinyGFX/Print.h` の整数・文字列版 | +260 B |
| `+ println(float)` | **約 +8,650 B → CH32V003 には載らない** |

### 9.8 拡張ヘッダに置く予定のもの（暫定）

| ヘッダ | 内容 | 状況 |
| --- | --- | --- |
| `TinyGFX/FontTiny.h` | TinyFont のデコーダ | 実装済み |
| `TinyGFX/FontU8g2.h` | u8g2 のデコーダ | 実装済み |
| `TinyGFX/Print.h` | `Print` 継承、`printf`、float | 実装済み |
| `TinyGFX/TileCanvas.h` | 帯レンダリング（§12） | 実装済み |
| `TinyGFX/Utf8.h` | UTF-8 の入口（CJK に要る。+153 B 実測） | 未（Q12） |
| `TinyGFX/TextDatum.h` | `setTextDatum` / `drawCenterString` | 未 |

いずれも **コアからは参照されない**。取り込んだ人だけが払う。

## 10. コンパイル時スイッチ一覧

| マクロ | 既定 | 効果 | 状況 |
| --- | --- | --- | --- |
| `TINYGFX_FILL_CHUNK` | 0 | `TinyGFXBusSPI` の塗りつぶしで使うスタック上の一時バッファ画素数 | 実装済み・**未測定** |
| `TINYGFX_FONT_SPARSE` | 1 | 0 で疎索引（コード表）の分岐を落とす | **−56 B**（[FONT_FORMAT.ja.md](FONT_FORMAT.ja.md) §4） |
| `TINYGFX_FONT_RECORDS` | 1 | 0 で可変ピッチ（グリフ表）の分岐を落とす | **−24 B** |
| `TINYGFX_STATIC_BUS` / `TINYGFX_STATIC_PANEL` | — | Bus / Panel を単一実装に固定して virtual を消す | **未実装。** 構成 E が予算内に収まったので保留（D2） |
| `TINYGFX_NO_CLIP` | — | クリップ判定を省く | **未実装。** 効果を測ってから決める |

**スイッチは増やしすぎない。** 追加するときは「これで何バイト減るか」を
[FOOTPRINT.ja.md](FOOTPRINT.ja.md) に測った数字とともに書く。測っていないスイッチは入れない。

## 11. 命名方針

**決めの問題でしかない名前は LovyanGFX に合わせる。** 移植互換のためではなく、**利用者が名前を覚え直さなくて済むようにするため。**

| 合わせるもの | `fillScreen` `clear` `drawPixel` `drawFastHLine` `fillRect` `drawRoundRect` `fillCircle` `pushImage` `setAddrWindow` `writeColor` `writePixels` `startWrite` `endWrite` `setRotation` `width` `height` `setCursor` `setTextColor` `setTextSize` `drawString` `textWidth` `color565` |
| --- | --- |
| **コアには置かず拡張ヘッダへ回すもの** | `print` / `println` / `printf`、float 版 `setTextSize`、`setTextDatum` / `drawCenterString`、`color888` |
| **合わせないもの（理由あり）** | `setColorDepth`（RGB565 固定）、`createSprite` 系（RAM がないので持たない）、`setFont` の引数型（独自の `TinyGFXFont`） |

**互換表は作らない。** LovyanGFX の仕様に追随する義務を負わないため。

## 12. 帯レンダリング（TinyGFXTileCanvas）

フレームバッファを持たないので、消してから描くとちらつく。全画面バッファは
240x240 RGB565 で 115KB あり載らない。そこで**画面を横帯に分け、小さな RAM バッファに
1 帯ずつ描いてから転送する**（LGFXVirtualCanvas と同じ考え方）。

### 12.1 構造 — コアには手を入れていない

```text
TinyGFXTileCanvas
  ├── TinyGFXPanelMemory   ← TinyGFXPanel の実装。RAM バッファへ書く
  │     └── TinyGFX        ← 描画コアはこれに向かって描く（コアは何も知らない）
  └── 実パネル（ST7789 など）← 帯ができたらここへ転送する
```

Panel が既に仮想化されているので、**新しい抽象を 1 つも足さずに実現できる**
（[DECISIONS.ja.md](DECISIONS.ja.md) D16）。

### 12.2 使い方

```cpp
static uint16_t band[240 * 2];                       // 幅 × 行数
TinyGFXTileCanvas canvas(panel, band, sizeof(band) / 2);
canvas.setBackgroundColor(TFT_BLACK);                // 各帯の初期状態
canvas.render(scene, &state);                        // scene が帯の数だけ呼ばれる
```

- 帯の行数は「バッファ画素数 ÷ 画面幅」で自動的に決まる（除算命令が無いので引き算で求める）
- 描画コールバックの座標は**常に画面全体のもの**。オフセットとクリップはこちらで隠す
- `canvas.gfx()` の設定（フォント・色）は `render()` をまたいで保持される

### 12.3 コストと制約

| 項目 | 実測 / 内容 |
| --- | --- |
| Flash | **+1,064 B**（使わなければ 0。別ヘッダなので include しなければリンクされない） |
| RAM | **幅 × 行数 × 2 バイト**。240px × 1 行 = 480 B、× 2 行 = 960 B |
| 制約 | 描画コールバックが帯の数だけ呼ばれる。重い前処理を中に書くとその回数走る |
| 制約 | 帯をまたぐ「隣の画素に依存する描画」はできない（アンチエイリアス等。今は持っていないので影響なし） |

### 12.4 テストにも効く

`TinyGFXPanelMemory` を**全画面サイズ**で使えば、ホスト上でそのままフレームバッファ検証になる。
描画コードを関数にする形が強制されるので、同じ関数を実機とテストの両方から呼べる
（[TEST_PLAN.ja.md](TEST_PLAN.ja.md) §2）。


## 13. I2C とモノクロパネル（SSD1306）

**コアには手を入れていない。** Bus と Panel を足しただけで動く。

### 13.1 I2C は Bus に収まる

SSD1306 系の I2C は「制御バイト + ペイロード」なので、既存の Bus インターフェースに
そのまま乗る。

| Bus のメソッド | I2C での中身 |
| --- | --- |
| `writeCommand(c)` | `beginTransmission` → `0x00` → `c` → `endTransmission` |
| `writeData(p, n)` | `beginTransmission` → `0x40` → ペイロード → `endTransmission`（**Wire のバッファ上限で分割**） |
| `beginTransaction` / `endTransaction` | **空。** I2C は転送ごとに start/stop する |
| `writeColor` / `writePixels` | **使わない。** モノクロパネルは `writeData` だけ使う |

制御バイトは `setControlBytes()` で変えられるので、同じ流儀の他のコントローラにも使える。

### 13.2 モノクロは Panel が引き受ける

**描画 API は RGB565 のまま。** `TinyGFXPanelSSD1306` が「0 でなければ点灯」で 1bpp に落とす。
色深度の抽象化はコアに入れない（[DECISIONS.ja.md](DECISIONS.ja.md) D4 / D21）。

SPI のカラーパネルとの違いは 2 つある。

| | SPI カラーパネル | SSD1306 |
| --- | --- | --- |
| フレームバッファ | **不要**（画面へ直接流す） | **必要**（GRAM を読み戻せず、ページ単位でしか書けない） |
| 転送のタイミング | 描いた瞬間 | **`display()` を呼んだとき** |

```cpp
static uint8_t fb[128 * 64 / 8];        // 1,024 バイト。利用者が用意する
TinyGFXBusI2C bus(0x3C);
TinyGFXPanelSSD1306 panel(bus, fb, 128, 64);
TinyGFX lcd(panel);

lcd.fillRect(8, 8, 32, 8, TFT_WHITE);
panel.display();                        // ここで初めて流れる
```

**変更のあったページだけ流す。** 1 ページ（縦 8 画素）に収まる変更なら 128 バイトで済む。
毎回 1KB 流すと I2C 400kHz で 25ms かかるので、これは効く。

### 13.3 回転はバッファの添字で行う

SSD1306 は 90 度回転を持たない（180 度は SEGREMAP/COMSCANDEC でできる）。
**バッファは自分で持っているので、添字で回す**ほうが一貫する。0〜3 すべて対応。

### 13.4 実測（[FOOTPRINT.ja.md](FOOTPRINT.ja.md) §6.4）

**フラッシュは SPI + ST7789 より小さく、RAM はフレームバッファぶん増える。**

| | CH32V003 flash | RAM |
| --- | --- | --- |
| SPI + ST7789（構成 D） | 11,808 | 572 |
| **I2C + SSD1306（同じ描画）** | **11,476** | **1,612** |

RAM 1,612 / 2,048 = 79%。**CH32V003 でも載る**が残りは 436 バイト。
128x32 のパネルならフレームバッファが 512 バイトになるので余裕が出る。
