# TinyGFX

**A LovyanGFX-flavoured drawing API, cut down until it fits MCUs with very little flash and RAM.**

> 日本語: [README.ja.md](README.ja.md)

On a CH32V003 (16 KB flash, 2 KB RAM), **every feature together costs +6.5 KB**.
Anything you do not call costs nothing at all.

> ### Only one setup has run on real hardware
>
> **On 2026-08-28 this drove a physical display for the first time**: an M5Stack BASIC
> (ILI9342C). The host suite and the M5Stack hardware suite pass. But that is **the only configuration confirmed on
> real glass.**
>
> | | |
> | --- | --- |
> | Confirmed | ILI9342C over hardware SPI (ESP32), primitives, text, rotation 0, **on-target output matches the host** |
> | **Unconfirmed** | **ST7789 / SSD1306 / software SPI / I2C / CH32V003 / rotations 1-3 / tiled rendering** |
>
> **The hardware tests pass too** - what the library draws on a real M5Stack matches, pixel
> for pixel, a golden image produced on the host ([tests/hw/m5stack/](tests/hw/m5stack/)).
>
> The surest way to try it is **[examples/M5StackBasic](examples/M5StackBasic)** - **no wiring
> at all**. It puts the border, colour order, text orientation and primitives on one screen
> and prints how to fix each one if it comes out wrong. The procedure is M0 in
> [docs/MANUAL_TEST.ja.md](docs/MANUAL_TEST.ja.md) (Japanese). The API may still move.

## What makes it different

| | |
| --- | --- |
| **Unused features cost 0 bytes** | A sketch that only calls `fillScreen` contains no circles and no text. **A test checks this mechanically** |
| **No framebuffer required** | Drawing streams straight to the panel. Keep one only when you want to (see below) |
| **No dynamic allocation** | No `malloc`, `new` or `String`. Buffers are supplied by you |
| **It never begins the bus for you** | It calls neither `SPI.begin()` nor `Wire.begin()`, and picks no pins. **You hand it a bus you already set up**, so your settings survive |
| **Bus, panel and font format are all swappable** | and **the implementations you do not use are not linked in** |
| **Decisions are made from measurements** | every design call is backed by a number measured on a CH32V003 (`docs/FOOTPRINT.ja.md`) |

## Getting started

### A colour TFT over SPI (ST7789)

```cpp
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/PanelST7789.h>

TinyGFXBusSoftSPI  bus(/*sck*/5, /*mosi*/6, /*dc*/3, /*cs*/4);
TinyGFXPanelST7789 panel(bus, 240, 240, /*rst*/2);
TinyGFX            lcd(panel);

void setup() {
  panel.setGramSize(240, 320);  // controller-side GRAM of a 240x240 ST7789 module
  lcd.begin();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BLACK);
  lcd.fillRect(10, 40, 80, 20, TFT_GREEN);
}
void loop() {}
```

The default bus is **bit-banged SPI**, because the CH32V003 Arduino core ships no SPI
library at all. Where hardware SPI is available, swap in `<TinyGFX/BusSPI.h>` — the
drawing code does not change by a single line.

> **You begin SPI and Wire yourself and hand the instance over.** TinyGFX never
> initialises a bus, so an SD card - or anything else on the same wires - keeps
> working. Sharing happens the standard Arduino way, with beginTransaction and
> endTransaction around each burst. The only pins TinyGFX drives are DC and CS.

### A monochrome OLED over I2C (SSD1306)

```cpp
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/PanelSSD1306.h>
#include <Wire.h>

static uint8_t fb[128 * 64 / 8];        // 1,024 bytes, supplied by you

TinyGFXBusI2C       bus(Wire, /*address*/0x3C);
TinyGFXPanelSSD1306 panel(bus, fb, 128, 64);
TinyGFX             lcd(panel);

void setup() {
  Wire.begin();                         // the sketch owns the bus
  lcd.begin();
  lcd.fillRect(8, 8, 40, 16, TFT_WHITE);
  panel.display();                      // nothing reaches the screen until this
}
void loop() {}
```

Monochrome panels differ in exactly two ways: **they need a framebuffer**, and
**nothing is sent until `display()`** (which transfers only the pages that changed).
The drawing API is identical; colours collapse to 1bpp as "non-zero lights up".

More in [examples/](examples/).

## What is included

| Kind | Implementation | Header |
| --- | --- | --- |
| Bus | Software SPI (default, portable) | `TinyGFX/BusSoftSPI.h` |
| | Hardware SPI | `TinyGFX/BusSPI.h` |
| | I2C (Wire) | `TinyGFX/BusI2C.h` |
| | Software I2C (any two GPIOs) | `TinyGFX/BusSoftI2C.h` |
| | Command-stream capture (for verification) | `TinyGFX/BusCapture.h` |
| Panel (colour) | ST7789 | `TinyGFX/PanelST7789.h` |
| | ILI9342C (M5Stack Core / BASIC) | `TinyGFX/PanelILI9342.h` |
| | ILI9341 | `TinyGFX/PanelILI9341.h` |
| Panel (monochrome) | SSD1306 | `TinyGFX/PanelSSD1306.h` |
| | SH1106 | `TinyGFX/PanelSH1106.h` |
| Panel (other) | RAM buffer (offscreen, tiled rendering, tests) | `TinyGFX/PanelMemory.h` |
| Font | CellFont (external spec v1, for H≤16) | `TinyGFX/FontCell.h` |
| | u8g2 | `TinyGFX/FontU8g2.h` |
| Images | raw RGB565 / RLE / RLE+palette / 1bpp (horizontal, vertical) | `TinyGFX/Image.h` |
| Extras | Tiled rendering (flicker-free) | `TinyGFX/TileCanvas.h` |
| | `print` / `printf` / float | `TinyGFX/Print.h` |

Colour panels sit on `TinyGFX/PanelDcs.h` and monochrome ones on
`TinyGFX/PanelPaged.h`. **Adding another controller of the same family
measures +0 bytes**, so the panel list is cheap to extend.

**The software SPI and the software I2C are there for different reasons.**

| | In the CH32V003 core | Why it exists |
| --- | --- | --- |
| `SPI.h` | **absent** | **because it is absent.** `BusSoftSPI` is the default bus |
| `Wire.h` | **present** | **for the pins.** Hardware I2C only comes out on two fixed pins, and on a part with as few as this one has, those are often wanted elsewhere |

Which one is smaller flips by target (measured):

| | Wire | software |
| --- | ---: | ---: |
| CH32V003 | **8,052** | 8,452 (+400) |
| AVR | 5,608 | **4,164 (−1,444, and −217 of RAM)** |

**On AVR the software one is smaller** - Wire carries a buffer and an
interrupt-driven state machine. On the CH32V003 Wire is built into the core and
already present in an empty sketch, so it wins there instead.

The software I2C **needs external pull-ups**, as any I2C bus does.

**What you do not include is not linked in** — buses, panels and font formats alike.

## What it draws

```
drawPixel  drawFastHLine  drawFastVLine  drawLine  drawRect  fillRect  fillScreen  clear
drawCircle  fillCircle  drawRoundRect  fillRoundRect  drawTriangle  fillTriangle
pushImage (with a transparent variant)  drawBitmap (1bpp)  drawImage (generated)
setAddrWindow  writeColor  writePixels
setClipRect  setRotation  startWrite / endWrite
setFont  setCursor  setTextColor  setTextSize
drawChar  drawString  drawCenterString  drawRightString  textWidth  fontHeight
```

**What each of these costs is in [docs/API.md](docs/API.md)** - measured, so you
can budget before you write the sketch.

Names follow LovyanGFX wherever the name was the only thing at stake.
**This is not a compatibility layer.**

## Portability, and the speed you trade for it

### It never begins the bus for you

**A small thing that prevents a real problem.** Plenty of libraries set the bus
up themselves, and when one of those is handed

```cpp
Wire.begin(21, 22, 400000);   // your pins, your clock
display.begin();              // <- and this calls Wire.begin() inside
```

**your configuration is overwritten.** The pins revert to the defaults, the
clock drops, and the other sensors on that bus stop working - silently.

TinyGFX calls neither `SPI.begin()` nor `Wire.begin()` and picks no pins. It
**takes an instance you already prepared**.

```cpp
Wire.begin(21, 22, 400000);            // yours. TinyGFX does not touch it
TinyGFXBusI2C bus(Wire, 0x3C);
```

The only pins it drives are **DC and CS, which belong to that panel**.
Transfers are wrapped in `beginTransaction` / `endTransaction` the standard
Arduino way, so an SD card on the same wires keeps working.

**The page-addressed monochrome panels were found not to be doing this on
2026-08-29, and it is fixed.** I2C starts and stops on every transfer so it
never showed, and there was no SPI test for those panels. `tests/monospi/` now
checks that not one byte leaves outside a transaction.

### It uses nothing but the standard Arduino API

The complete list of what `src/` touches:

```
pinMode  digitalWrite  digitalRead  delay  delayMicroseconds
SPI   (only if you include <TinyGFX/BusSPI.h>)
Wire  (only if you include <TinyGFX/BusI2C.h>)
```

No vendor SDK, no register banging, no per-platform `#ifdef` (bar the PROGMEM
read, which is a plain dereference everywhere but AVR). **The drawing core does
not even reference `Arduino.h`** - only the bus implementations do.

That is why `library.properties` says `architectures=*`, and why it will
**probably work on anything Arduino runs on**. Measured on CH32V003, Arduino
Uno R3, ESP32 and the host.

### The price is that it is not fast

The portability is bought by using **nothing but plain GPIO and
`SPI.transfer()`**. There is no DMA, no register-level path, no per-platform
fast route. Measured on an M5Stack Core (320x240) at 24MHz, **with
`TINYGFX_FILL_CHUNK` switched off**:

| | |
| --- | --- |
| `fillScreen` | 178 ms |
| `fillRect` 100x100 | 23 ms |
| `fillCircle` r100 | 89 ms |
| `drawString`, 10 characters at size 2 | 3 ms |
| `TileCanvas`, one full frame | 194 ms |

**Setting `TINYGFX_FILL_CHUNK` to 1 or more makes fills faster** - it uses
Arduino's block write, `SPI.transfer(buf, len)`, and costs `size * 2` bytes of
stack and nothing else. The figures above are the floor without it, taken
before the `fillRect` seam went in, so today's should be a little better.

**Still not the library for smooth animation.** It is built for instruments, clocks
and settings screens - the kind that redraw what changed.

### If you want speed, write the bus yourself

Subclass `TinyGFXBus`. **The library is not touched at all.**

```cpp
class MyDmaBus : public TinyGFXBus {
 public:
  void init() override { /* pins and peripheral */ }
  void beginTransaction() override { /* CS low */ }
  void endTransaction() override { /* CS high */ }
  void writeCommand(uint8_t c) override { /* DC low, one byte */ }
  void writeData(const uint8_t* d, size_t n) override { /* n bytes */ }

  // **this is where the speed goes.** The same colour, count times
  void writeColor(uint16_t color, uint32_t count) override { /* DMA if you like */ }
  // **and here.** A run of pixels, as they are
  void writePixels(const uint16_t* d, uint32_t count) override { /* DMA if you like */ }
};

MyDmaBus bus;
TinyGFXPanelST7789 panel(bus, 240, 240, /*rst*/2);
TinyGFX lcd(panel);
```

`writeColor` and `writePixels` are where bulk transfers enter, and fills,
images and glyphs all end up there. **Not a line of your drawing code changes.**

The panel has a seam too: override `fillRect` and you can bypass the address
window entirely, which is what the monochrome panels do.

**A test can hold you to it** - `TinyGFXBusCapture` reassembles the bytes that
actually went out into a virtual GRAM, so a fast bus and a plain one can be
compared for pixel equality without any hardware (`tests/hostbus/` is that
shape).

## Footprint (CH32V003, `-Os`, measured)

An empty sketch is 5,892 bytes on this core, so the budget is tracked as the
**increment over that**.

| Cumulative | Δ flash | Δ RAM |
| --- | --- | --- |
| Bus + panel + `fillScreen` | +1,712 | +68 |
| + rectangles, pixels, H/V lines | +1,988 | +68 |
| + every primitive (lines, circles, rounded, triangles) | +4,880 | +68 |
| + text | +5,916 | +68 |
| **+ `pushImage` (everything)** | **+6,536** | **+68** |
| + tiled rendering (240px × 1 row) | +7,524 | +624 |
| + `print` / `println` (no float) | +6,200 | +80 |
| + `println(float)` | **does not fit** (~ +8,650) | — |

That last row is a measurement, not a policy: float formatting alone eats more than half
of the CH32V003's flash. **It is not forbidden** — pull in `TinyGFX/Print.h` and it works;
skip it and you pay nothing.

Confirmed targets: **CH32V003** (the reference board), **Arduino Uno R3**, **ESP32**.
Everything else is in `docs/FOOTPRINT.ja.md`.

## Flicker

With no framebuffer, clearing before drawing flickers. A full one is 115 KB at
240x240 RGB565 and simply does not fit, so instead the screen is **split into horizontal
bands, each rendered into a small RAM buffer and then pushed**.

```cpp
static uint16_t band[240 * 2];          // width × rows × 2 bytes
TinyGFXTileCanvas canvas(panel, band, sizeof(band) / sizeof(band[0]));

static void scene(TinyGFX& g, void* ctx) {   // called once per band
  g.fillCircle(120, 120, 40, TFT_CYAN);      // coordinates are full-screen
}
canvas.render(scene);
```

The RAM cost is **width × rows × 2 bytes** and nothing else (240px × 1 row = 480 bytes).
**Changing the row count does not change a single pixel** — a test enforces that.

## Fonts

**The library bundles no font data at all.** Fonts live in your sketch.

Generation is expected to go through
[LGFXFontToolJs](https://www.npmjs.com/package/lgfx-font-tool).

### Making one

The CLI bakes in **only the characters your project uses**.

```sh
npx -p lgfx-font-tool lgfx-font build --google "Noto Sans JP" --em 12 \
    --chars "温度設定完了 23.5℃" --format cellfont --out font.h
```

> **Write `npx -p lgfx-font-tool lgfx-font ...`.** The package is `lgfx-font-tool`
> but the command is `lgfx-font`, and plain `npx lgfx-font` **404s in some
> environments** - it depends on the npx cache and on whether a `node_modules`
> is around. CI uses the `-p` form for that reason.

Those twelve characters come to **245 bytes**. It grows with what you add.

The output is **plain CellFont** and mentions nothing from TinyGFX. Wrap it in
one line to hand it to `setFont()`:

```cpp
#include <TinyGFX.h>
#include <TinyGFX/FontCell.h>   // the decoder, for the format you use
#include "font.h"               // straight from the CLI, never hand-edited

static const TinyGFXFontRef myFont = {&font, &tinygfxFontCellOps, nullptr};

lcd.setFont(&myFont);
lcd.drawString("23.5", 8, 8);
```

`TinyGFX.h` brings in the CellFont types, so **include order does not matter**
(types only, zero bytes).

### Two formats

**The decoder for the one you do not use is not linked in.**

| Format | Suited to |
| --- | --- |
| **CellFont** (`FontCell.h`) | pixel-grid fonts **16 pixels tall or less**, or **few glyphs** |
| u8g2 (`FontU8g2.h`) | taller than that **and many glyphs**, where RLE and per-glyph bboxes pay |

**The CellFont format is specified outside TinyGFX** — see `docs/formats/cellfont.ja.md` in
[LGFXFontToolJs](https://github.com/tanakamasayuki/LGFXFontToolJs). TinyGFX is one renderer
for it. The two decoders are nearly the same size (684 B and 693 B), so the choice comes
down to data. To mix, say, Latin and CJK, **generate the whole character set in one go** -
the tool splits it by width class internally and hands you one font.

## Installing

Not in the Arduino Library Manager yet (unreleased). For now, drop a ZIP or a clone into
your `libraries/` folder.

```cpp
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>     // the bus you use
#include <TinyGFX/PanelST7789.h>    // the panel you use
```

## Documentation

The design record is Japanese only; [docs/README.ja.md](docs/README.ja.md) is the index.

| To read about | Document |
| --- | --- |
| What the library is for, and where its responsibility ends | [docs/REQUIREMENTS.ja.md](docs/REQUIREMENTS.ja.md) |
| The API shape and internal structure | [docs/CORE_DESIGN.ja.md](docs/CORE_DESIGN.ja.md) |
| **Why it is built this way (reasons, and the options not taken)** | [docs/DECISIONS.ja.md](docs/DECISIONS.ja.md) |
| Flash and RAM budgets, and the measurements | [docs/FOOTPRINT.ja.md](docs/FOOTPRINT.ja.md) |
| **Every call, and what each one costs** | **[docs/API.md](docs/API.md)** |
| **How this differs from the other GFX libraries, and why** | **[docs/COMPARISON.md](docs/COMPARISON.md)** |
| Font measurements (the format itself is specified elsewhere) | [docs/FONT_FORMAT.ja.md](docs/FONT_FORMAT.ja.md) |
| Image format measurements | [docs/IMAGE_FORMAT.ja.md](docs/IMAGE_FORMAT.ja.md) |
| Test strategy | [docs/TEST_PLAN.ja.md](docs/TEST_PLAN.ja.md) |
| **What to check on real hardware** | [docs/MANUAL_TEST.ja.md](docs/MANUAL_TEST.ja.md) |
| Where the project stands and what is left | [docs/DEVELOPMENT_PLAN.ja.md](docs/DEVELOPMENT_PLAN.ja.md) |

## Tests

```sh
cd tests && uv sync && uv run pytest -v -s
```

No hardware needed: everything either runs on the host core or just builds and inspects
size and symbols. Details in [tests/README.md](tests/README.md).

Some characteristic tests:

- **`linkprune/`** — `nm` proves that unused features and unused font formats are absent from the final binary
- **`footprint/`** — per-configuration increments stay within budget, and **the numbers are always printed**
- **`tile/`** — changing the band height must not change a single pixel
- **`hostbus/`** — captures what the real SPI bus actually put on the wire and turns it back into an image
- **`i2c/`** — the same, over I2C to an SSD1306
- **`hw/m5stack/`** — **real hardware (Tier 3).** What a real M5Stack draws is compared to a host-made golden
- **`clifont/`** — a CellFont header **from the real generator** renders correctly
- **`fillchunk/`** — block-writing must not change a single byte on the wire
- **`fontchain/`** — a font later in the chain is still reachable past an earlier notdef (**verified by deliberately breaking it**)
- **`ili9342/`** — MADCTL, colour order and mirroring (**whether the table is right on real
  glass is what M0 checks**)

## License

MIT — see [LICENSE](LICENSE).
