# When a panel does not look right, what to change

> 日本語: [PANEL_TUNING.ja.md](PANEL_TUNING.ja.md)

**A panel is a preset.** Include one from the catalogue and it usually just
works ([GLOSSARY.md](GLOSSARY.md)).

This is for when it does not. **Look up the symptom, find what to change.**

There are three kinds of fix.

| | What it takes | Examples |
| --- | --- | --- |
| **A runtime call** | one line, anywhere | inversion, mirroring, colour order, offset |
| **A different include** | pick another panel | COM pin layout (`_SeqCom`) |
| **A `#define`** | one or two lines before the include | COM pin layout, pre-charge, VCOMH |

**The COM pin layout, the pre-charge and VCOMH cannot be changed at runtime.**
Those need a rebuild.

## How to use it

1. **Get something on the screen first.** If anything at all is showing, look
   the symptom up below
2. **Change one thing at a time.** Once it is right, find the catalogue entry
   that carries that value, or add your own (§4)
3. **Tell us what worked.** In the catalogue, nobody has to look it up again

## 1. When nothing appears at all

Before the table. **Usually this is not a settings problem.**

| Check | How |
| --- | --- |
| **Did you begin the bus?** | TinyGFX calls neither `SPI.begin()` nor `Wire.begin()` ([DECISIONS.ja.md](DECISIONS.ja.md) D24). **That one is yours** |
| **Did `begin()` return true?** | `false` means "this configuration is unusable": a null buffer, a height that is not a multiple of 8, or a zero dimension |
| **The buffer size** | Write `static uint8_t fb[TinyGFXPanelSSD1306_128x64::kBufferBytes];` and it cannot be wrong. Work it out by hand and a buffer that is too small still passes `begin()`, then overruns |
| **Did you call `display()`?** | A page-addressed panel **sends nothing until you do**. Drawing alone puts nothing on the glass |
| **The I2C address** | 0x3C or 0x3D, often selected by a resistor on the back of the module |
| **The wiring** | Pin numbers for software SPI; pull-ups for I2C |

## 2. The symptom table

**Ordered by how easy it is to judge by eye.**

| What you see | What to change | How |
| --- | --- | --- |
| **Black and white are swapped** | inversion | `panel.invertDisplay(false);` **after** `begin()` |
| **Red and blue are swapped** (colour) | colour order | `panel.setRgbOrder(false);` |
| **Mirrored left to right** (colour) | mirror X | `panel.setMirror(false, false);` |
| **Upside down** | mirror Y / scan direction | colour: `panel.setMirror(mx, !my);` |
| **Shifted a few pixels sideways, with a stripe of rubbish down an edge** | column offset | `panel.setColumnOffset(2);` (mono) or `panel.setOffset(x, y);` (colour) |
| **Every other row is a stripe, or the halves are swapped** | **COM pin layout** | try the `_SeqCom` entry in the catalogue (§3) |
| **Squashed into the top half of the glass** | height | pick a different panel; the multiplex ratio follows the height |
| **Rotation 0 is fine but 2 and 3 are offset** | GRAM size | `panel.setGramSize(240, 320);` (colour) |
| **Dim, or flickering** | pre-charge, VCOMH | §4. **Hard to judge by eye**, so leave it for last |

### Inversion and mirroring go after `begin()`

`invertDisplay`, `setSleep` and `displayOn` **put a command straight on the
bus**. Called before `begin()`, either the bus is not up yet or `begin()`'s own
init sequence overwrites them.

`setOffset`, `setGramSize`, `setMirror`, `setRgbOrder` and `setColumnOffset` can
be called **whenever you like**, in any order - they touch no bus.

## 3. The COM pin layout is the one thing that cannot be guessed

**It is effectively the only value a catalogue preset really carries.**

How the glass's COM lines are wired. There are two.

| Value | Meaning | Symptom |
| --- | --- | --- |
| `0x12` | alternative. **The datasheet reset value** | — |
| `0x02` | sequential | wrong one gives every other row as a stripe, or swapped halves |

**The height does not decide it.** A 128x32 is sequential, and a 64x32 - the
same 32 rows - is alternative. **This is why TinyGFX does not guess it from the
height**: it used to, and it was wrong for both of those.

Both are in circulation at 128x64, so the catalogue has two.

| | |
| --- | --- |
| `panels/SSD1306_128x64.h` | alternative (the common one) |
| `panels/SSD1306_128x64_SeqCom.h` | sequential |

**If you see stripes, try the other one.** With two choices it either fixes it
or it does not.

## 4. Adding your own panel

When the catalogue has nothing for yours, or to freeze a value you found above.

Copy an entry into the catalogue in `tools/gen_panels.py`. **A header written by
hand is undone by the next generator run**, and a test says so.

```python
dict(driver="SSD1306", w=128, h=64, suffix="MyModule", com_pins=SEQUENTIAL,
     note="where it came from, and how to recognise it"),
```

There are only four fields, because **everything else is derived or is the
controller's reset value** ([GLOSSARY.md](GLOSSARY.md) §4).

| Field | If you leave it out |
| --- | --- |
| `com_pins` | `0x12`, the datasheet reset value |
| `col0` | computed as "the glass sits centred in memory" |
| `gram` / `offset` (colour) | same as the panel size / `(0, 0)` |
| `suffix` | none |

### Changing pre-charge and VCOMH

**Only if the brightness or the flicker bothers you.** There is no way to judge
the right value by eye, which is why no preset carries one. Define it in the
sketch:

```cpp
#define TINYGFX_SSD1306_PRECHARGE 0xF1   // default is the reset value, 0x22
#define TINYGFX_SSD1306_VCOMH     0x40   // default is the reset value, 0x20
#include <TinyGFX/panels/SSD1306_128x64.h>
```

> **The `0xF1` and `0x40` that other libraries send appear in no datasheet
> table.** `0x40` is not among the VCOMH values (`00h` / `20h` / `30h`) at all.
> One library's tuning has been copied across the field, so TinyGFX defaults to
> **the reset values**. If it comes out too dim, the two lines above get you
> back to what everyone else sends.

## 5. Reading back what is connected

**Colour panels only, and only where MISO is actually wired.** You can read the
controller's ID.

```cpp
uint8_t id[3];
panel.readId(id);   // an ILI9341-family part answers something like 00 93 41
```

All `00` or all `FF` means **the data line never comes back**. On a board like
the M5Stack Core, where SDO does not reach the SPI MISO pin, it cannot be read
([MANUAL_TEST.ja.md](MANUAL_TEST.ja.md)).

**A monochrome I2C OLED cannot be read at all.** It is write-only.

**TinyGFX does not use this to detect your panel.** Too few combinations of bus
and panel can be read, and a library compiled for a known board has no need of
runtime detection. **It is a diagnostic for when you do not know what you have.**
