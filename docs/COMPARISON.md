# How TinyGFX compares to the other GFX libraries

> 日本語: [COMPARISON.ja.md](COMPARISON.ja.md)

A record of what the major graphics libraries do, made to check whether
TinyGFX's API and design hold up. The point is not "everyone else does X" but
**why they do it** - so that where TinyGFX differs, the reason can be stated.

## What was surveyed

| Library | Version | How |
| --- | --- | --- |
| **Adafruit_GFX** | installed | source |
| **U8g2** | 2.36.19 | **source + measured benchmarks** - the only one the same picture was written in twice |
| **LovyanGFX** | 1.2.26 | source |
| **M5GFX** | 0.2.27 | source |
| **TFT_eSPI** | — | README |
| **Arduino_GFX** (moononournation) | — | README |
| **GxEPD2** | — | README |
| **embedded-graphics** (Rust) | — | documentation |

## At a glance

| | TinyGFX | Adafruit_GFX | U8g2 | LovyanGFX | TFT_eSPI | Arduino_GFX |
| --- | --- | --- | --- | --- | --- | --- |
| Drawing calls | **47** | 64 | 126 | **175** | many | many |
| Virtuals in the core | **0** | 18 | 0 (C function pointers) | 3 | few | few |
| Colour depths | **RGB565 only** | 1 / 8 / 16 | 1bpp | **9** (1/2/4/8 palette, grey 8, 565/666/888/8888) | 1 / 8 / 16 | several |
| Panels | **5** | separate libraries | many | **~45** | 18+ | **40+** |
| Buses | 3 (soft SPI / SPI / I2C) | driver's business | SPI / I2C / parallel | **9** | SPI / 8-bit / 16-bit parallel | SPI / 8-bit / 16-bit / RGB |
| Platforms | **`architectures=*`** | wide | wide | **17** | 5 families | ~20 |
| Header-only | **yes** | no | no | no | no | no |

**TinyGFX is the smallest on every axis.** That is the point; the survey is not
looking for gaps to fill but checking that **each gap has a reason that can be
stated out loud**.

## The axes

### 1. What a driver has to implement

This is where the libraries differ most.

| | Required of a driver | Optional, for speed |
| --- | --- | --- |
| **Adafruit_GFX** | `drawPixel` (pure virtual) - **that is all** | `drawFastHLine` / `drawFastVLine` / `fillRect` / `writePixel` and more |
| **U8g2** | `ll_hvline` (a horizontal or vertical run into the buffer) | — |
| **embedded-graphics** | `draw_iter` (take a sequence of pixels) - **that is all** | `fill_contiguous` / `fill_solid` / `clear` |
| **LovyanGFX** | the whole `Panel_Device` surface | many |
| **TinyGFX** | `setWindow` + `writeColor` + `writePixels` | **`fillRect`** |

**"One required method plus optional specialisations" is exactly what
embedded-graphics does.** TinyGFX's `fillRect` seam is not a novel idea; the
same problem has produced the same answer twice.

**Adafruit_GFX's single required `drawPixel` is the easiest to port to and the
slowest to run.** The default `fillRect` places pixels one at a time, which is
orders of magnitude off on a panel that can be streamed - so real drivers end
up overriding it anyway.

**Only U8g2 makes the primitive a run** rather than a window. For a
page-addressed 1bpp buffer that is the natural fit
([OPTIMIZE.ja.md](OPTIMIZE.ja.md) H).

### 2. Colour

**TinyGFX has no colour-depth abstraction in the core**
([DECISIONS.ja.md](DECISIONS.ja.md) D4). The API is always `uint16_t` RGB565,
and the SSD1306 panel is what collapses it to 1bpp with "non-zero lights up".

LovyanGFX switches between nine depths at runtime. Adafruit splits them into
`GFXcanvas1` / `8` / `16`.

**Both carry two of everything, and neither would fit the reference board.**
But **this decision will need revisiting to add a greyscale or palette panel**.
An SSD1327 (4bpp) added today would have the panel reduce 565 to 4bpp, by which
point the shades are already gone.

### 3. Buffering

| | |
| --- | --- |
| **U8g2** | a page buffer with `firstPage` / `nextPage` - the "picture loop" |
| **GxEPD2** | the same shape. Its README says **"paged drawing is implemented as picture loop, like in U8G2"**. Page count is a template argument |
| **Adafruit_GFX** | `GFXcanvas1/8/16` - offscreen canvases |
| **LovyanGFX / TFT_eSPI** | `Sprite` / `LGFX_Sprite`, with a choice of depth |
| **TinyGFX** | `TileCanvas` (bands) and `MemoryTarget` (offscreen) |

**Band rendering is the same idea as U8g2's and GxEPD2's.** In TinyGFX the
invariant is pinned by test: a band-drawn frame is byte-identical to a
whole-buffer one (`tests/tile/`, `tests/i2c/`).

**A sprite is already possible with `TinyGFXMemoryTarget`** - build a `TinyGFX`
on a RAM buffer, draw into it, then `pushImage()` it to the screen. There is
simply no call named `pushSprite`. **That is a documentation gap, not a missing
feature.**

### 4. Text - the "two families" problem

**LovyanGFX and Adafruit_GFX both carry two sets of calls that do the same
job.**

| | Family A | Family B |
| --- | --- | --- |
| **LovyanGFX** | `setCursor` + `print` / `println` (flowing) | `drawString(s, x, y)` (placed) |
| | `drawCenterString(s, cx, y)` | `setTextDatum(TC)` + `drawString` ← **the same thing twice** |
| **Adafruit_GFX** | `drawPixel` / `drawLine` … (each opens its own transaction) | `writePixel` / `writeLine` … (assume the caller called `startWrite`) |

TinyGFX has neither duplication.

- **Alignment**: `drawCenterString` / `drawRightString` only, no
  `setTextDatum`. **A datum is state `drawString` must consult every time**, so
  it drags `textWidth` in for everyone, including those who never align
  anything. Measured on a CH32V003: **+204 B against +0 B** ([API.md](API.md))
- **draw / write pairs**: `startWrite()` / `endWrite()` **count their nesting**,
  so opening one on the outside simply nests the drawing inside it. No second
  family needed
- **UTF-8**: U8g2 splits here too - `drawStr` reads bytes, `drawUTF8` decodes.
  TinyGFX has one `drawString` and **UTF-8 is the default**
  ([DECISIONS.ja.md](DECISIONS.ja.md) D26). Byte-per-character is
  `TINYGFX_FONT_UTF8=0`: **a build choice rather than a second function**, so
  there is no pair of calls to pick wrongly between

### 4.5 What looks like two families but is not

TinyGFX has two entrances for images too - `drawBitmap` and `drawImage` - but
**that is a different thing from the duplication above.**

- `setTextDatum` + `drawString` and `drawCenterString` take **the same input
  and produce the same result**. One of them is enough
- `drawBitmap` and `drawImage` differ in **where the data comes from**: the
  first takes the raw array any converter on the internet emits, with no build
  step; the second takes a self-describing struct whose encoding a dedicated
  tool chose

`drawBitmap` measures cheaper for the simple case (308 against 556 on a
CH32V003) and using both overlaps by −56. **Call neither and both cost
nothing.**

### 5. Print

**`Adafruit_GFX : public Print`** - inheritance, so the base class always
carries Print's interface.

TinyGFX keeps it in `TinyGFX/Print.h`. **Do not include it and not one byte
arrives.** That is not tidiness: `print(float)` **overflows the reference
board's flash by 2,724 bytes** (measured), so it cannot live in the base.

### 6. Reading the panel back

**Arduino_GFX states "no read operations" as a design decision** - for
footprint and speed, and because not every panel can be read.

TinyGFX can read, but through `TinyGFXDriverDcs::readPixels()`, an inline member
that **is not emitted unless called**. The only thing everyone pays is the bus's
`readSequence`, measured at 8 bytes. At **150us a pixel** it is a debugging and
verification tool, never a drawing path.

It earns its place: the hardware golden comparison (`tests/hw/m5stack/`) and the
automated check that rotation maps correctly (M2) are impossible without it.

## U8g2 - the one that was actually measured against

The others were read. **U8g2 was benchmarked**: the same picture written twice,
once in each library, and both compiled. It is the most used monochrome library
and the closest in intent to TinyGFX.

### The same picture, the same board (Arduino Uno, SSD1306 128x64 over I2C)

A frame, a filled box, a circle, a line, a triangle and `"1234"`. Measured
2026-08-29.

| | flash | RAM |
| --- | ---: | ---: |
| **TinyGFX (whole-screen buffer)** | **8,036** | 1,406 |
| **TinyGFX (one-page band)** | **8,040** | **510** |
| U8g2 (`_F_`, whole screen) | 11,020 | 1,626 |
| U8g2 (`_1_`, paged) | 11,018 | 730 |
| (reference) TinyGFX drawing nothing | 5,308 | 1,380 |
| (reference) U8g2 drawing nothing | 8,064 | 1,556 |

- **2,984 B smaller in total**
- **2,756 B smaller before drawing anything.** U8g2 carries the u8x8 layer and
  its controller descriptions
- **220 B less RAM in banded mode** (510 against 730)
- **Banding costs almost no flash** (+4). U8g2's paging is structural, so it is
  even there

### How it is built

U8g2's core is that there is **exactly one primitive**.

```c
u8g2_DrawHVLine(u8g2, x, y, len, dir)   // a horizontal or vertical run
```

Boxes, circles, bitmaps and glyphs all funnel into it, and clipping happens
there once. Below it, three layers reached through C function pointers.

| Layer | Implementation | Size |
| --- | --- | ---: |
| clipping | `u8g2_DrawHVLine`, one of them | 342 |
| rotation | `draw_l90_r0` … `r3` plus `mirrorr_r0` | 32 (r0) |
| **buffer layout** | `ll_hvline_vertical_top_lsb` / `..._horizontal_right_lsb` | 218 |

**There are only two low-level writers, and they are named after the buffer
layout, not the colour depth.** There is no "monochrome" layer anywhere
([OPTIMIZE.ja.md](OPTIMIZE.ja.md) H).

Function pointers, so no virtuals. It also keeps **`u8x8`, a separate
text-only layer**, for uses that need no graphics at all.

### Fonts - run-length encoded

U8g2's font format **stores runs** (`bits_per_0` / `bits_per_1`), so its decoder
reads a run length and calls `DrawHVLine`. CellFont is a raw bitmap, so the
TinyGFX decoder scans bits to find the runs.

**The obvious conclusion - "switch to RLE and it shrinks" - did not survive
measurement.** Swapping only the format inside the same implementation (AVR,
code size):

| | CellFont (raw bitmap) | u8g2 format (RLE) |
| --- | ---: | ---: |
| total | **1,218** | **1,170** |

**48 bytes.** Losing the bit scan takes 254 B off `draw`, and reading bit fields
and the index puts 370 B back. **CellFont spec 13.2's "do not compress" call is
supported by our own measurement.**

### Who wins what

| | |
| --- | --- |
| **U8g2** | **the font decoder.** About 310 B against roughly 1,000 for the same job. Only 48 B of that is the format; the rest is how tightly it is written |
| | breadth of panels - almost every 1bpp controller there is |
| | `u8x8`, a genuinely lighter text-only layer |
| **TinyGFX** | 2,984 B in total, 2,756 B before drawing, 220 B of banded RAM |
| | header-only: **an API you do not call costs nothing** |
| | the same API drives colour panels (U8g2 is monochrome only) |

### What TinyGFX took from it

**The decision to make `fillRect` a seam came from reading U8g2.** So did the
framing that a buffered panel wants runs while a direct-GRAM panel wants
rectangles ([OPTIMIZE.ja.md](OPTIMIZE.ja.md) H).

Measured: the seam took 76-92 B off the monochrome build on a CH32V003.

## Worth taking (measured)

The survey turned up **one** addition clearly worth making.

| | Cost on a CH32V003 | Who has it |
| --- | ---: | --- |
| `drawNumber` | +168 B | LovyanGFX, TFT_eSPI |
| ~~`setTextWrap`~~ | **+164 B, and added** (D33) | Adafruit |
| `drawEllipse` / `fillEllipse` | not measured | Adafruit, LovyanGFX |
| `drawArc` | not measured | LovyanGFX, TFT_eSPI |

**The 1bpp bitmap is the strongest case.** It is the natural format for an icon
and, on a monochrome panel, far more direct than a 16bpp `pushImage`. All three
other libraries have it, so compatibility argues for it too.

**Not added yet.** It goes in when someone needs it.

## Not taking

| | Why |
| --- | --- |
| Runtime colour-depth switching | brings back carrying two of everything (D4). To revisit for a greyscale panel |
| `setTextDatum` | costs +204 B to people who never use it (measured) |
| Inheriting `Print` | `print(float)` overflows the reference board by 2,724 B (measured) |
| draw / write pairs | unnecessary once `startWrite()` counts its nesting |
| Image decoders (JPG / PNG / BMP / QOI) | the decoder alone would use up the reference board's flash |
| Anti-aliasing (TFT_eSPI, LovyanGFX) | needs intermediate shades. Meaningless at 1bpp, and needs the depth abstraction |
| Touch (LovyanGFX has 11 controllers) | out of scope ([REQUIREMENTS.ja.md](REQUIREMENTS.ja.md) 5) |
| `setSwapBytes` (**every library has it**) | a runtime mode costs **+44 B and +4 B of RAM to sketches that never swap** (a branch per pixel). `tinygfx_swapBytes565()` is called instead - **0 B unless called** (D29) |

## Holes this survey found in TinyGFX

1. ~~Offscreen drawing is undocumented~~ - **written up** (2026-08-29).
   `MemoryTarget` + `pushImage` is the sprite. **No API was added; what already
   existed was documented.** See [API.md](API.md)
2. ~~No 1bpp bitmap~~ - **added** (2026-08-29) as `drawBitmap`. It costs
   **+284 B**; the +120 estimate was for a naive per-pixel version. Coalescing
   runs into one `fillRect` each costs 84 B more and saves an order of
   magnitude of window setups on a colour panel
3. ~~**No `setTextWrap`.**~~ → **added** (2026-08-29, D33).
   `TINYGFX_TEXT_WRAP=1` brings out `TinyGFXPrint::setTextWrap()`.
   **Off by default**, and off costs nothing. On costs **+164 B** (measured).
   The price is that wrapping has to know how wide a character is *before*
   drawing it, which links a second entry point into the font decoder
4. **The colour decision is right for now, not forever.** Adding an SSD1327
   (4bpp) or a greyscale e-paper breaks the "let the panel reduce 565" model.
   **That is when D4 needs revisiting**
