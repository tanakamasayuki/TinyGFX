# examples

> 日本語: [README.ja.md](README.ja.md)

| Example | What it shows | Flash added on CH32V003 |
| --- | --- | --- |
| [HelloWorld](HelloWorld) | A rectangle and some text. **Start here** | ~6.0 KB (through text) |
| [Shapes](Shapes) | Every primitive | ~4.9 KB |
| [FlickerFree](FlickerFree) | Tiled rendering, no flicker, no framebuffer | ~7.6 KB / ~2.0 KB RAM |
| [HardwareSPI](HardwareSPI) | The same drawing over hardware SPI | about the same as Shapes |
| [OledI2C](OledI2C) | **A monochrome I2C OLED** (SSD1306) | ~5.6 KB / **~1.1 KB RAM** |

Numbers come from [../docs/FOOTPRINT.ja.md](../docs/FOOTPRINT.ja.md) (Japanese).
**Features you do not call are not linked in**, so call only what you need.

## Monochrome OLEDs work differently

[OledI2C](OledI2C) differs in two ways.

1. **It needs a framebuffer** — 1,024 bytes for 128x64, supplied by you (512 for a 128x32 panel)
2. **Nothing reaches the screen until `panel.display()`** — and only the pages that changed
   are sent, so a change confined to one page (8 rows) costs 128 bytes

The drawing API is the same as for colour panels; colours collapse to 1bpp as "non-zero lights up".

## Wiring

Every example declares its pins at the top. Change them to match your board.

| Signal | Role |
| --- | --- |
| SCK / MOSI | SPI. Not declared in the hardware-SPI example — the core owns them |
| DC | Command / data select |
| CS | Chip select. `-1` if the panel has the bus to itself |
| RST | Reset. `-1` if the module handles it |

## Fonts live in your sketch

**TinyGFX ships no font data.** `HelloWorld` bundles `tinygfx_font5x7.h`, but that is a
stopgap 5x7 face covering only `0x20`-`0x3F`. Real fonts are generated with
[LGFXFontToolJs](https://www.npmjs.com/package/lgfx-font-tool).

The format is GFXfont (Adafruit GFX compatible), so existing Adafruit font headers can be
handed to `setFont()` unchanged.

**On AVR the font must be in PROGMEM**, otherwise it eats RAM and renders as garbage.

## Panel origin offset

240x240 and 135x240 ST7789 modules are smaller than the controller's GRAM, so their origin
is shifted. Set both values:

```cpp
panel.setGramSize(240, 320);   // the controller's GRAM
panel.setOffset(52, 40);       // where the visible area sits at rotation 0
```

Rotations 1-3 are derived from those.
