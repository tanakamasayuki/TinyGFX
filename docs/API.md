# API reference

> 日本語: [API.ja.md](API.ja.md)

Everything `TinyGFX` exposes, and **what each call costs**.

For how to use the library see [../README.md](../README.md); for why it is
built this way see [DECISIONS.ja.md](DECISIONS.ja.md); for the footprint
picture as a whole see [FOOTPRINT.ja.md](FOOTPRINT.ja.md).

## The price list

**An API you do not call costs nothing at all.** The library is header-only and
the drawing core has not one virtual method, so unused code never reaches the
linker in the first place ([DECISIONS.ja.md](DECISIONS.ja.md) D1). That is what
makes a table like this worth printing.

Measured on the reference board, a CH32V003 at `-Os`. **Each figure is the
increment over a sketch that already has the panel, the bus and `fillRect`**
(7,772 B).

| API | Cost | |
| --- | ---: | --- |
| `fillScreen` / `clear` | **48** | `fillRect` under another name |
| `drawPixel` | **56** | a 1x1 `fillRect` |
| `drawFastHLine` / `drawFastVLine` | **60** | likewise |
| `setRotation` | **72** | writes the panel's MADCTL |
| `setClipRect` / `resetClipRect` | **152** | |
| `drawRect` | 284 | |
| `fillCircle` | 388 | |
| `pushImage` | 392 | |
| `drawBitmap` (1bpp) | 284 | runs coalesced into one `fillRect` each |
| `drawLine` | 436 | Bresenham |
| `drawCircle` | 472 | |
| `drawTriangle` | 520 | |
| `pushImage` (transparent) | 608 | |
| `fillRoundRect` | 612 | |
| `fillTriangle` | **832** | the scanlines have to be sorted |
| `drawRoundRect` | **868** | **dearer than filling one.** Four arcs to draw |
| Text (`drawChar` / `drawString` / `textWidth`) | **1,028-1,132** | see below |
| `TinyGFXPrint` + `print(long)` | +280 | on top of text |
| `print(float)` | **does not fit** | see below |

### How to read it

**The numbers do not add up.** Everything sits on `fillRect` and shares code
with everything else. Using `drawCircle` and `fillCircle` together does not
cost 472 + 388, and `drawString` is only +92 over `drawChar` (1,028).

Two entries are worth remembering.

- **`drawRoundRect` costs more than `fillRoundRect`** - 868 against 612. A fill
  is scanlines; an outline is four arcs drawn separately. If all you want is a
  rounded frame, filling one and painting the inside in the background colour
  can come out smaller.
- **Text starts at 1,028 B.** Nearly all of that is the CellFont decoder, and
  `textWidth` alone pulls in 1,044 B because it still has to walk the index and
  the chain. **Draw one character and you have paid for all of it**; everything
  after that is rounding error.

### What will not fit

**`print(float)` and `println(float)` are not expensive, they are impossible.**
On a CH32V003 the flash region **overflows by 2,724 bytes** (measured). They fit
on AVR and ESP32, but an API that does not fit on the reference board is not one
this library will recommend.

If you only need an integer, `print(long)` costs +280 B. If you need a decimal
point, splitting the value yourself (`v / 100` and `v % 100`) is smaller
everywhere, on every core.

## Contracts

### What `begin()` tells you

```cpp
bool begin();
```

**The return value means "this configuration can work", not "a panel
answered".** Every panel TinyGFX drives is write-only in normal use, and a
4-wire SPI display has no acknowledgement at all. Reading `true` as "the screen
is alive" will mislead you.

It returns false only for the class of mistake a compiler cannot catch: a null
framebuffer, a height that is not a whole number of pages, a zero dimension.
**Nothing is sent when it does.**

### Coordinates may be off-screen

Every drawing call **accepts coordinates outside the screen and clips them** -
negative ones, ones at the ends of `int16_t`, absurd widths.

```cpp
lcd.fillRect(-100, -100, 32767, 32767, TFT_RED);  // fills the screen
```

When the clipped rectangle is empty, not one byte goes out.

### `startWrite()` / `endWrite()`

```cpp
void startWrite();   // nests
void endWrite();
```

Opens and closes the bus transaction. **Nesting is counted**, so two
`startWrite()` calls need two `endWrite()` calls.

Individual drawing calls open and close their own, so **you normally do not
need these**. Wrapping a long run of drawing in one pair saves the per-call
open and close.

### The bus belongs to the sketch

**TinyGFX never calls `SPI.begin()` or `Wire.begin()`, and never picks pins.**

```cpp
SPI.begin();          // yours to own
lcd.begin();
```

That is what lets an SD card share the same wires. Transactions are wrapped the
standard Arduino way.

### The framebuffer belongs to the sketch

The page-addressed monochrome panels (SSD1306, SH1106) **allocate nothing**.

```cpp
static uint8_t fb[128 * 64 / 8];              // 1,024 B
TinyGFXPanelSSD1306 panel(bus, fb, 128, 64);
```

When that will not fit, draw a band at a time instead - 128 B for one page.
[FOOTPRINT.ja.md](FOOTPRINT.ja.md) 6.4 has the numbers.

## The calls

### Basics

| | |
| --- | --- |
| `TinyGFX(TinyGFXPanel& panel)` | |
| `bool begin()` | see the contract above |
| `int16_t width()` / `height()` | **after rotation** |
| `void setRotation(uint8_t r)` / `uint8_t getRotation()` | 0..3; 1 and 3 swap width and height |
| `static constexpr uint16_t color565(r, g, b)` | folded at compile time |

### Shapes

All take `uint16_t color` last; coordinates are `int16_t`.

| | |
| --- | --- |
| `drawPixel(x, y, c)` | |
| `fillRect(x, y, w, h, c)` / `fillScreen(c)` / `clear(c = 0)` | **the core.** Almost everything else goes through it |
| `drawFastHLine(x, y, w, c)` / `drawFastVLine(x, y, h, c)` | |
| `drawRect` / `drawLine` | |
| `drawCircle` / `fillCircle` (cx, cy, r, c) | |
| `drawRoundRect` / `fillRoundRect` (x, y, w, h, r, c) | see the note in the price list |
| `drawTriangle` / `fillTriangle` (x0..y2, c) | |

### Clipping

| | |
| --- | --- |
| `setClipRect(x, y, w, h)` | |
| `resetClipRect()` / `clearClipRect()` | the same thing; the second is an alias |

### Images

| | |
| --- | --- |
| `pushImage(x, y, w, h, const uint16_t* data)` | RGB565, row major |
| `pushImage(x, y, w, h, data, uint16_t transparent)` | that colour is skipped |
| `drawBitmap(x, y, const uint8_t* bitmap, w, h, color)` | **1bpp, for icons. +284 B** |

`pushImage` data must be **in RAM**. PROGMEM images on AVR are not supported
([DECISIONS.ja.md](DECISIONS.ja.md) Q6).

`drawBitmap` is the other way round: it **reads the way font data is read**
(`tinygfx_rd8`), so on AVR it must be **in PROGMEM**. Bits are MSB first and
**every row starts on a byte boundary** - `(w + 7) / 8` bytes to a row. That is
the layout Adafruit_GFX, U8g2 and LovyanGFX all use, so an icon converter's
output drops straight in.

```cpp
static const uint8_t icon[] TINYGFX_FONT_PROGMEM = {
  0x18, 0x24, 0x42, 0x81, 0x81, 0x42, 0x24, 0x18,
};
lcd.drawBitmap(10, 10, icon, 8, 8, TFT_WHITE);
```

Only the 1 bits are painted; 0 bits are left alone. To paint the background as
well, fill the rectangle first - cheaper than carrying a second colour through
the loop.

**Runs of set bits become one `fillRect` each**, so a panel that took the
`fillRect` seam serves this too. That costs 84 B more than placing pixels one at
a time, and saves an order of magnitude of window setups on a colour panel.

### Drawing offscreen (what other libraries call a sprite)

**There is no separate API.** `TinyGFXPanelMemory` presents a RAM buffer as a
panel, so you build a `TinyGFX` on it, draw, and `pushImage()` the result.

```cpp
static uint16_t sprBuf[16 * 16];              // 512 B
TinyGFXPanelMemory sprPanel(sprBuf, 16, 16);
TinyGFX spr(sprPanel);

spr.begin();
sprPanel.fillBuffer(TFT_BLACK);
spr.fillCircle(8, 8, 6, TFT_RED);             // every drawing call works here

lcd.pushImage(10, 10, 16, 16, sprBuf);                  // paste it
lcd.pushImage(10, 50, 16, 16, sprBuf, TFT_BLACK);       // paste it, black transparent
```

`TinyGFXPanelMemory` also has `readPixel(x, y)` and `fillBuffer(color)`.

**Two bytes a pixel, so mind the size.** On the reference board (CH32V003, 2 KB
of RAM) 16x16 is 512 B and **32x32 does not fit at 2,048 B** (measured). If what
you want is a whole screen without flicker, use the band rendering in
[`TinyGFX/TileCanvas.h`](../src/TinyGFX/TileCanvas.h) instead of a sprite.

### Text

Needs `#include <TinyGFX/FontCell.h>`. **Leave it out and the decoder is not
built** - `tinygfxFontCellOps` is simply not declared and the sketch will not
compile.

The confusing part is that a **similarly named header does come in on its own**.

| Header | What is in it | Pulled in by `TinyGFX.h` |
| --- | --- | --- |
| `TinyGFX/CellFont.h` | **types and macros only** (the `CellFont` struct, `CELLFONT_READ_*`) | **yes. Measured at 0 bytes** |
| `TinyGFX/Font.h` | `TinyGFXFontRef` / `TinyGFXFontOps` | **yes**, through `Gfx.h` |
| `TinyGFX/FontCell.h` | **the decoder** (about 1,000 B) | **no** |

`CellFont.h` comes in by default because a generated font header **includes
nothing itself** and stops with an `#error` when the types are missing (CellFont
spec 12.2). Without that you get an ordering trap - "including the font before
`TinyGFX.h` breaks the build". It is types and macros, so the price is zero.

| | |
| --- | --- |
| `setFont(const TinyGFXFontRef*)` / `getFont()` | |
| `drawString(const char* s, x, y)` | returns how far the pen moved |
| `drawCenterString(s, cx, y)` | centred on `cx`. **+116 B**, nothing if never called |
| `drawRightString(s, rx, y)` | right edge at `rx`. **+232 B for both** |
| `drawChar(uint16_t ch, x, y)` | |
| `textWidth(const char* s)` | **1,044 B on its own** |
| `setTextColor(fg)` / `setTextColor(fg, bg)` | the two-argument form paints the cell behind |
| `setTextSize(uint8_t)` / `getTextSize()` | whole multiples only |
| `setCursor(x, y)` / `getCursorX()` / `getCursorY()` | for `TinyGFXPrint` |
| `fontHeight()` / `getTextLineHeight()` / `getTextAscent()` | |
| `getTextColor()` / `getTextBgColor()` / `hasTextBg()` | **for font decoders.** A decoder is a free function handed a `TinyGFX&`, so this is how it reads the text state. A normal sketch has no use for them |

`y` is the **top of the line**, not the baseline.

A character the font does not cover falls back to U+FFFD. If the font has no
U+FFFD either, nothing is drawn - there is no built-in tofu box.

### The raw window

| | |
| --- | --- |
| `setAddrWindow(x, y, w, h)` | |
| `writeColor(uint16_t c, uint32_t count)` | |
| `writePixels(const uint16_t* data, uint32_t count)` | |

Straight to the panel. **Clipping does not apply** - keeping inside the panel is
your problem.

## Macros

Everything defaults to on. **Define nothing and nothing changes.**

| Macro | Default | Setting it to 0 | CH32V003 |
| --- | --- | --- | ---: |
| `TINYGFX_FONT_BG` | 1 | the second argument of `setTextColor` stops working | −108 |
| `TINYGFX_FONT_SCALE` | 1 | `setTextSize(2)` and above stop working | −116 |
| `TINYGFX_FONT_CHAIN` | 1 | `CellFont::next` is not followed | −16 |
| `TINYGFX_FONT_SPARSE` | 1 | a sparse font **cannot be drawn** | font dependent |
| `TINYGFX_FONT_RECORDS` | 1 | a variable-pitch font **cannot be drawn** | font dependent |
| `TINYGFX_MONO_FAST_FILL` | 1 | monochrome fills go back to one pixel at a time | −428 |
| `TINYGFX_FILL_CHUNK` | 0 | (1 or more enables block writes; speed only) | — |

**`SPARSE` and `RECORDS` have to match the font, or the wrong glyphs are
drawn.** The encoding is runtime data, invisible to the compiler; the `Format :`
line in a generated header is the only clue.

## What is deliberately absent

The LovyanGFX family
([LGFXVirtualCanvas](https://github.com/tanakamasayuki/LGFXVirtualCanvas)) has
around 150 calls where TinyGFX has 51. **Not a gap to be filled** - the point is
to not carry what will not fit on the reference board.

| Absent | Why |
| --- | --- |
| `drawFloat` / `print(float)` | **overflows the reference board by 2,724 B** (measured). `dtostrf` is not in the CH32V003 core at all |
| `drawJpg` / `drawPng` / `drawBmp` / `drawQoi` | the decoder alone would use up the reference board's flash |
| `pushImageRotateZoom` / `setPivot` | rotate-and-scale needs floating point or fixed-point interpolation |
| `drawArc` / `drawEllipse` / `drawBezier` | trigonometry or parametric curves. Measure first if someone asks |
| `setTextDatum` / `getTextDatum` | **deliberately absent.** Alignment is `drawCenterString` / `drawRightString` instead - see below |
| `drawNumber` | **+168 B would buy it** (measured). Integer to string |
| Palettes, switchable colour depth | the core has no colour-depth abstraction (D4) |
| `readPixel` | it lives on the panel: `TinyGFXPanelDcs::readPixels()`. **150us a pixel**, for debugging |

The last two are **measured, so they can be added the moment they are wanted.**
Not before.

### Why there is no `setTextDatum`

LovyanGFX offers text alignment **twice** - as `drawCenterString()`, and as
`setTextDatum(TC_DATUM)` followed by `drawString()`. Two families for one job.

TinyGFX keeps only the calls. The reason is the price, measured on a CH32V003:

| | a sketch that draws text but never aligns it |
| --- | ---: |
| no alignment feature at all | 8,892 |
| with `setTextDatum` | 9,096 (**+204**) |
| with `drawCenterString` / `drawRightString`, never called | 8,892 (**+0**) |

**A datum is state that `drawString` has to consult every time**, so
`drawString` drags `textWidth` in unconditionally - and everyone who never
centres anything pays for it. The calls are inline members: never called, never
emitted.
