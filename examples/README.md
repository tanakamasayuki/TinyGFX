# examples

> 日本語: [README.ja.md](README.ja.md)

| Example | What it shows | Flash added on CH32V003 |
| --- | --- | --- |
| [HelloWorld](HelloWorld) | A rectangle and some text. **Start here** | ~6.0 KB (through text) |
| [Shapes](Shapes) | Every primitive | ~4.9 KB |
| [FlickerFree](FlickerFree) | Tiled rendering, no flicker, no framebuffer | ~7.6 KB / ~2.0 KB RAM |
| [HardwareSPI](HardwareSPI) | The same drawing over hardware SPI | about the same as Shapes |

Numbers come from [../docs/FOOTPRINT.ja.md](../docs/FOOTPRINT.ja.md) (Japanese).
**Features you do not call are not linked in**, so call only what you need.

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
