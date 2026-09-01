# Glossary

> 日本語: [GLOSSARY.ja.md](GLOSSARY.ja.md)

One place for the words TinyGFX uses. **This is the first document a reader
reaches for**, so it exists in both languages, like API.md.

**The vocabulary and the code agree** (the rename landed on 2026-08-30).

## 1. What you choose

You only ever write two names: a bus and a panel.

### Bus

**How bytes get to the panel.**

**There are two kinds, and what separates them is what you hand over - not
whether it is hardware or software.**

| Kind | You hand it | Whose implementation | Who begins it | Class |
| --- | --- | --- | --- | --- |
| **Peripheral bus** | an Arduino peripheral (`SPIClass&` / `TwoWire&`) | **the core's** | **you** | `TinyGFXBusSPI` / `TinyGFXBusI2C` |
| **Pin bus** | pin numbers | **TinyGFX's** (bit-banged with `pinMode` / `digitalWrite`) | **TinyGFX** | `TinyGFXBusSoftSPI` / `TinyGFXBusSoftI2C` |

#### A peripheral bus is yours

You call `SPI.begin()` or `Wire.begin()` and hand over an instance that is
already up. TinyGFX never begins it ([DECISIONS.ja.md](DECISIONS.ja.md) D24). An
SD card on the same wires just works - pass it the same `SPI`.

**The only pins TinyGFX drives are DC and CS**, both of which belong to that
panel alone.

#### A pin bus is TinyGFX's

TinyGFX takes the pins you name, makes them outputs with `pinMode` and drives
them itself. Software SPI holds all four - SCK, MOSI, DC and CS. **Nothing else
may use them.**

It exists for the pins, not for the size. A hardware peripheral only comes out
on fixed pins, and on a part as pin-poor as a CH32V003 those may be wanted for
something else.

#### What "Soft" means

**The "Soft" in `TinyGFXBusSoftSPI` means "TinyGFX bit-bangs it itself". It does
not mean "this is the only software one".**

What `TinyGFXBusSPI` accepts is whatever the core calls `SPI`, and **TinyGFX
neither knows nor needs to know whether that is a hardware peripheral.** Some
cores ship an `SPI` that is implemented in software; you can also hand it your
own software SPI derived from `SPIClass`. Either is just a peripheral bus.

**That is why the split is not "hardware vs software".** The axis is ownership:
what you hand over stays yours, and what you do not hand over TinyGFX drives.

### Panel

**The product you bought** - "a 0.96 inch 128x64 I2C OLED".

Named after its controller and its size, `TinyGFXPanelSSD1306_128x64`. Pick one
from the catalogue and include it.

```cpp
#include <TinyGFX/panels/SSD1306_128x64.h>
static uint8_t fb[TINYGFX_PANEL::kBufferBytes];
TINYGFX_PANEL panel(bus, fb);
```

**Including a panel also names it**, so the class name is written once.
**Two panels and `TINYGFX_PANEL` disappears** - there is no longer one panel it
could mean, so the second header to arrive takes the name away. Write the class
names then.

A panel is a preset (§3), not a driver (§2). **The same driver behind different
glass is a different panel**, with a different multiplex ratio, column offset and
COM wiring.

**One panel per driver per sketch.** Including two panels of the same driver
stops the build (§3). Two different drivers - a TFT and an OLED - are fine.

## 2. The layers underneath

You never write these names. They exist so that the documentation can point at a
layer.

### Target

**What the drawing core hands pixels to.** Something that accepts a window and
pixels.

`TinyGFXTarget`. It is the `x` in `TinyGFX lcd(x)`.

A panel is a target, but **so is a RAM buffer** - which is why no word meaning
"controller IC" would do here. Tiled rendering (`TinyGFXTileCanvas`) slots a RAM
target in front of a panel, and that only works because the target is an
abstraction.

### Protocol

**The command language** - what byte sequence carries a picture to the panel.
**There are only two.**

| Protocol | What it is | Drivers |
| --- | --- | --- |
| **DCS protocol** | Window plus pixels: MIPI DCS `0x2A` CASET, `0x2B` RASET, `0x2C` RAMWR. Colour is RGB565 | ST7789 / ST7735 / ST7796 / ILI9341 / ILI9342 |
| **Page protocol** | Page plus bits: one byte is 8 pixels tall. Owns a framebuffer and sends it on `display()` | SSD1306 / SH1106 |

**Adding a controller usually adds no protocol.** An ST7796 or a GC9A01 speaks
the DCS protocol, so all that grows is the panel catalogue - not one line of
library code.

**The word does not appear in class names**; it is vocabulary only. What exists
in code is `TinyGFXDriverDcs` and `TinyGFXDriverPaged`, the shared half of every
driver that speaks that protocol.

### Driver

**The controller IC.** SSD1306, ST7789, SH1106.

`TinyGFXDriverSSD1306`: a protocol's shared half plus that IC's own quirks.

"Driver" is what the datasheets call themselves.

- SSD1306: "Segment/Common **Driver** with Controller"
- ST7789: "Single-Chip **Controller/Driver** for 262K-Color TFT-LCD"

**Same protocol, different transfer, different driver.** An SSD1306 and an
SH1106 both speak the page protocol, but the SSD1306 takes a range (`0x21` /
`0x22`) and accepts the whole run, while an SH1106 has no such command and needs
the cursor placed per page. **That difference is code, not data**, so it makes
them separate drivers.

### How they stack

```
sketch
  |  chooses
Bus ──────────── how bytes travel
  |
Panel ────────── what you bought          <- preset (data)
  |  which is a
Driver ───────── which IC                 <- library (code)
  |  implements a
Protocol ─────── which command language   <- library (code)
  |  satisfies
Target ───────── takes a window and pixels  <- abstraction
  ^
drawing core (TinyGFX)
```

## 3. Configuration

### Preset

**The set of values a panel header carries.** The catalogue ships with the
library.

**This is the opposite of the font policy.** TinyGFX bundles no font data at all
([DECISIONS.ja.md](DECISIONS.ja.md) D17); it does bundle panel presets. Neither
reason for keeping fonts out applies here.

| | Font | Panel preset |
| --- | --- | --- |
| Size | hundreds to thousands of bytes | **about 30 bytes** |
| Decided by | which characters this project uses | **which product you bought** (a finite catalogue) |
| Without it | choosing characters takes a step | **you cannot get a first picture** |

**A preset holds nothing that can be derived.** As §4 shows, almost every value
either follows from the size of the glass or is the controller's own reset value.

### Including two is refused

Including two panels of the same driver **stops the build with `#error`.**

Without that, `#pragma once` silently ignores the second one and **the second
panel is driven with the first one's values.** A picture appears, so the error
goes unnoticed. **Better to fail the build than to be quietly wrong.**

The guard is **per driver**, so a TFT and an OLED together still build.

## 4. What a panel is configured with

**Most of it is derivable, or is the controller's reset value.** Only the ★
entries have to be carried by a preset.

### Page protocol (SSD1306 / SH1106)

| Setting | Command | Where the value comes from |
| --- | --- | --- |
| Multiplex ratio | `0xA8` | **derived: height − 1** (matches all nine known variants) |
| Column offset | — | **derived: the glass sits centred in RAM**, `(RAM width − width) / 2`. ★ overridden on the variants that are not centred |
| **COM pin layout** | `0xDA` | ★ **one of two.** The datasheet reset value is `0x12` (alternative); the other is `0x02` (sequential). **A physical fact about how the glass is wired** |
| Pre-charge | `0xD9` | datasheet reset value `0x22` (phase 1 = 2, phase 2 = 2) |
| VCOMH | `0xDB` | datasheet reset value `0x20` (0.77 × Vcc) |
| Clock divide | `0xD5` | datasheet reset value |

### DCS protocol (ST7789 / ILI934x …)

| Setting | Command | Where the value comes from |
| --- | --- | --- |
| GRAM origin offset | added to `0x2A` / `0x2B` | the module |
| GRAM size | — | the controller (240x320 and so on) |
| Colour order RGB / BGR | `0x36` bit 3 | how the glass is wired |
| Inversion | `0x21` / `0x20` | the panel's character (IPS is usually inverted) |
| Mirror X / Y | `0x36` bits 6/7 | how the module is mounted |
| Dummy read bits | before `0x2E` | the controller |

## 5. Hardware words

| Word | Meaning |
| --- | --- |
| **GRAM** | The controller's image memory. **It can be larger than the glass** - a 135x240 panel on a 240x320 GRAM - which is why an origin offset exists |
| **Multiplex ratio** | How many COM lines get scanned. Effectively the height |
| **COM pin layout** | How the glass's COM lines are arranged: sequential or alternative. Get it wrong and every other row is a stripe |
| **Segment remap** | Which way round it is horizontally. `0xA0` / `0xA1` |
| **COM scan direction** | Which way round it is vertically. `0xC0` / `0xC8` |
| **MADCTL** | DCS `0x36`. Rotation, mirroring and colour order in one byte |
| **Tab** | **An ST7735 habit only.** Modules ship with a coloured pull-tab on the protective film - green, red, black - and the colour names the variant. No other panel has anything like it, so TinyGFX does not use the word |

## 6. Everything else

| Word | Meaning |
| --- | --- |
| **Drawing core** | The `TinyGFX` class. Not one virtual method ([DECISIONS.ja.md](DECISIONS.ja.md) D1) |
| **Tiled rendering** | `TinyGFXTileCanvas`. Splits the screen into horizontal bands and draws one at a time into a small RAM buffer. What keeps it from flickering |
| **Price tag** | How many bytes of flash a feature adds, **measured on the reference board** ([FOOTPRINT.ja.md](FOOTPRINT.ja.md)) |
| **Reference board** | CH32V003 (16 KB flash / 2 KB SRAM). When optimisations disagree across targets, this one wins |
