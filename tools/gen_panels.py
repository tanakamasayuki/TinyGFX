#!/usr/bin/env python3
"""Generate the panel catalogue into src/TinyGFX/panels/.

    python3 tools/gen_panels.py            # write the headers and the READMEs
    python3 tools/gen_panels.py --check    # fail if anything is out of date

It also rewrites the catalogue table in README.md and README.ja.md, and the
panel entries in keywords.txt, between their markers. **A hand-maintained list of two dozen panels goes stale on
the first addition**, so it is generated from the same table as the headers.

**A panel is a preset - the product someone bought.** The library ships the
catalogue because, unlike a font, there is a right answer for "the 0.96 inch
128x64 OLED everyone has", and without it nobody gets a first picture
(docs/DECISIONS.ja.md D34).

## What an entry carries

Almost nothing, and that is the point. Everything the driver can work out for
itself stays out of here:

    multiplex ratio     height - 1, on every module ever measured
    pre-charge, VCOMH   the controller's own reset values
    clock divide        the controller's own reset value

What is left is what a datasheet cannot tell you about *your* glass:

    com_pins   how the COM lines are wired. Two choices, and the height does
               not predict which: a 64x32 wants alternative while a 128x32
               wants sequential. Omit it for the datasheet reset (alternative)
    col0       where column 0 of the picture sits in the controller's memory.
               Derived as "the glass sits centred in RAM", which is right for
               every module measured but the 96x16. Omit it unless it is not
    offset     the same idea for a colour panel, but NOT derivable: a 240x240
               ST7789 sits at the top of its 240x320 memory while a 135x240
               sits centred. Always explicit

## Naming

`<Driver>_<width>x<height>`, and **when two panels share a driver and a size,
the difference goes in the name** - `SSD1306_128x64_SeqCom`. No mapping to any
other library's names: nothing here is called `noname` or `winstar`, because
those cannot be read without knowing the library they came from.
"""

import argparse
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / "src" / "TinyGFX" / "panels"

SEQUENTIAL = 0x02  # COM lines wired in order
ALTERNATIVE = 0x12  # COM lines interleaved (the datasheet reset)

# --- the catalogue ---------------------------------------------------------
#
# `note` is what a reader needs to recognise their own module, and what to try
# when the picture is wrong. It is not a changelog.

PAGED = [
    dict(driver="SSD1306", w=128, h=64,
         note="The 0.96 inch module sold on every marketplace, I2C or SPI.\n"
              "If yours is 0.91 inch it is the 128x32 next door."),
    dict(driver="SSD1306", w=128, h=64, suffix="SeqCom", com_pins=SEQUENTIAL,
         note="A 128x64 whose COM lines are wired sequentially.\n"
              "Try this one when the 128x64 comes out as every other row."),
    dict(driver="SSD1306", w=128, h=32, com_pins=SEQUENTIAL,
         note="The 0.91 inch module. Half the height, and it wires its COM\n"
              "lines sequentially rather than alternately."),
    dict(driver="SSD1306", w=96, h=16, com_pins=SEQUENTIAL, col0=0,
         note="The 0.69 inch bar. **The one module that does not sit centred\n"
              "in memory** - it is flush left, so the derived offset is wrong\n"
              "for it and is overridden here."),
    dict(driver="SSD1306", w=96, h=40, com_pins=SEQUENTIAL, col0=0,
         note="A 96x40. Like the 96x16 it sits flush left rather than centred,\n"
              "so the derived offset is overridden here."),
    dict(driver="SSD1306", w=72, h=40,
         note="The 0.42 inch module."),
    dict(driver="SSD1306", w=64, h=48,
         note="The 0.66 inch module."),
    dict(driver="SSD1306", w=64, h=32,
         note="A 64x32. **Note it wants the alternative COM layout despite\n"
              "being 32 tall**, which is why the layout is not guessed from\n"
              "the height."),
    dict(driver="SSD1306", w=48, h=64,
         note="The 0.71 inch portrait module."),
    dict(driver="SH1106", w=128, h=64,
         note="The 1.3 inch module, sold as an \"SSD1306\" more often than not.\n"
              "132 columns of memory behind 128 of glass, so the picture sits\n"
              "two columns in - which the driver derives, the glass being\n"
              "centred."),
]

DCS = [
    dict(driver="ST7789", w=240, h=240, gram=(240, 320), offset=(0, 0),
         note="The 1.3 inch square module. Its memory is 240x320, so the GRAM\n"
              "size has to be told apart from the panel size or rotations 2\n"
              "and 3 land 80 rows out."),
    dict(driver="ST7789", w=135, h=240, gram=(240, 320), offset=(52, 40),
         note="The size a TTGO T-Display uses. Small glass centred in a large\n"
              "memory: both the GRAM size and the offset are needed, and\n"
              "either alone gets rotation wrong."),
    dict(driver="ST7789", w=240, h=320, gram=(240, 320), offset=(0, 0),
         note="The full 2.0 inch panel, filling its memory exactly."),
    dict(driver="ST7789", w=240, h=280, gram=(240, 320), offset=(0, 20),
         note="The 1.69 inch module. Full width, 40 rows short of the memory,\n"
              "and centred down it."),
    dict(driver="ST7789", w=170, h=320, gram=(240, 320), offset=(35, 0),
         note="The 1.9 inch bar, centred across its memory."),
    dict(driver="ST7789", w=172, h=320, gram=(240, 320), offset=(34, 0),
         note="The 1.47 inch bar, centred across its memory."),
    # An ST7735 is the same chip behind at least four pieces of glass. The
    # memory is 132x162 and the panel sits somewhere inside it, so **the offset
    # is what separates these entries** - along with the colour order on one.
    dict(driver="ST7735", w=128, h=160, gram=(132, 162), offset=(2, 1),
         note="The 1.8 inch module. Glass centred in the 132x162 memory."),
    dict(driver="ST7735", w=128, h=160, suffix="FlushRgb", gram=(132, 162),
         offset=(0, 0), rgb=True,
         note="A 1.8 inch whose glass is flush with the memory **and wired RGB\n"
              "rather than BGR**. Try this when the 128x160 comes out shifted\n"
              "two pixels with red and blue swapped."),
    dict(driver="ST7735", w=128, h=128, gram=(132, 162), offset=(2, 3),
         note="The 1.44 inch square module. **Not centred vertically** - it\n"
              "sits 3 rows down a 162-row memory, not 17."),
    dict(driver="ST7735", w=80, h=160, gram=(132, 162), offset=(26, 1),
         note="The 0.96 inch bar, centred in the memory."),
    dict(driver="ST7735", w=80, h=160, suffix="Offset24", gram=(132, 162),
         offset=(24, 0),
         note="A 0.96 inch bar that sits at column 24 rather than centred.\n"
              "Try this when the 80x160 is shifted a couple of pixels."),
    dict(driver="ST7796", w=320, h=480, gram=(320, 480), offset=(0, 0),
         note="The 3.5 inch breakout. Its memory is exactly the panel, so\n"
              "nothing is offset."),
    dict(driver="ILI9341", w=240, h=320, gram=(240, 320), offset=(0, 0),
         note="The 2.4 and 2.8 inch SPI breakouts. Portrait memory, inversion\n"
              "off, and mounted mirrored on X - which is what makes rotation 0\n"
              "want MADCTL 0x48."),
    dict(driver="ILI9342", w=320, h=240, gram=(320, 240), offset=(0, 0),
         note="M5Stack Core and BASIC. Landscape memory from the start, so\n"
              "nothing is offset. Older BASIC units come up inverted;\n"
              "invertDisplay(false) after begin() is the fix."),
]

GUARD = '''#pragma once
#ifdef TINYGFX_DRIVER_{driver}_INCLUDED
#error "TinyGFX: one {driver} panel per sketch. Include this header rather than <TinyGFX/Driver{driver}.h>, and do not include a second panels/{driver}_*.h - the second would be silently ignored and its panel driven with the first one's values."
#endif
'''

HEAD = '''// {cls}
//
{note}//
// Generated by tools/gen_panels.py - edit the catalogue there, not this file.
// If the picture is wrong, docs/PANEL_TUNING.ja.md says which line to change.
'''

PAGED_BODY = '''#include "../Driver{driver}.h"

class {cls} : public TinyGFXDriver{driver} {{
 public:
  static const int16_t kWidth = {w};
  static const int16_t kHeight = {h};
  /// Size the framebuffer with this, so it cannot be got wrong:
  ///   static uint8_t fb[{cls}::kBufferBytes];
  static const uint16_t kBufferBytes = (uint16_t)(kWidth * kHeight / 8);

  /// `bufferPages` renders in a band of that many pages; 0 is the whole screen.
  {cls}(TinyGFXBus& bus, uint8_t* buffer, int16_t bufferPages = 0)
      : TinyGFXDriver{driver}(bus, buffer, kWidth, kHeight, bufferPages) {{{body}}}
}};
'''

DCS_BODY = '''#include "../Driver{driver}.h"

class {cls} : public TinyGFXDriver{driver} {{
 public:
  static const int16_t kWidth = {w};
  static const int16_t kHeight = {h};

  explicit {cls}(TinyGFXBus& bus, int8_t rst = -1)
      : TinyGFXDriver{driver}(bus, kWidth, kHeight, rst) {{{body}}}
}};
'''


def name(e):
    n = f"{e['driver']}_{e['w']}x{e['h']}"
    return n + "_" + e["suffix"] if e.get("suffix") else n


def render(e, paged):
    cls = "TinyGFXPanel" + name(e)
    defines = ""
    body = ""
    if paged:
        if "com_pins" in e:
            defines = ("\n// This glass wires its COM lines sequentially, not alternately.\n"
                       if e["com_pins"] == SEQUENTIAL else "\n// COM pin layout for this glass.\n")
            defines += f"#define TINYGFX_{e['driver']}_COM_PINS 0x{e['com_pins']:02X}\n"
        if "col0" in e:
            body = (f"\n    // Not centred in memory, so the derived offset does not apply.\n"
                    f"    setColumnOffset({e['col0']});\n  ")
    else:
        gw, gh = e["gram"]
        ox, oy = e["offset"]
        if (gw, gh) != (e["w"], e["h"]):
            body += f"\n    setGramSize({gw}, {gh});"
        if (ox, oy) != (0, 0):
            body += f"\n    setOffset({ox}, {oy});"
        if "rgb" in e:
            body += "\n    setRgbOrder(false);   // this glass is wired RGB"
        if body:
            body += "\n  "
    note = "".join("// " + ln + "\n" for ln in e["note"].split("\n"))
    tmpl = PAGED_BODY if paged else DCS_BODY
    return (HEAD.format(cls=cls, note=note)
            + GUARD.format(driver=e["driver"])
            + defines
            + tmpl.format(driver=e["driver"], cls=cls, w=e["w"], h=e["h"], body=body))


# --- the catalogue table in the READMEs ------------------------------------

BEGIN, END = "<!-- BEGIN PANEL TABLE -->", "<!-- END PANEL TABLE -->"

# One line per driver, ordered the way someone shopping would read it.
FAMILY_JA = {
    "ST7789": "ST7789", "ST7735": "ST7735", "ST7796": "ST7796",
    "ILI9341": "ILI9341", "ILI9342": "ILI9342C",
    "SSD1306": "SSD1306", "SH1106": "SH1106",
}
KIND_JA = {True: "モノクロ（1bpp）", False: "カラー（RGB565）"}
KIND_EN = {True: "Monochrome (1bpp)", False: "Colour (RGB565)"}
ORDER = ["ST7789", "ST7735", "ST7796", "ILI9341", "ILI9342", "SSD1306", "SH1106"]


def size_label(e):
    """`240x240`, or `240x240 (SeqCom)` when a suffix separates two entries."""
    n = f"{e['w']}x{e['h']}"
    return f"{n} ({e['suffix']})" if e.get("suffix") else n


def readme_table(ja):
    rows = []
    for drv in ORDER:
        for entries, paged in ((DCS, False), (PAGED, True)):
            hits = [e for e in entries if e["driver"] == drv]
            if not hits:
                continue
            sizes = " / ".join(f"`{size_label(e)}`" for e in hits)
            kind = (KIND_JA if ja else KIND_EN)[paged]
            rows.append(f"| **{FAMILY_JA[drv]}** | {kind} | {sizes} |")
    head = ("| ドライバ | 種類 | パネル（`TinyGFX/panels/<ドライバ>_<寸法>.h`） |\n| --- | --- | --- |"
            if ja else
            "| Driver | Kind | Panels (`TinyGFX/panels/<driver>_<size>.h`) |\n| --- | --- | --- |")
    return head + "\n" + "\n".join(rows)


KW_BEGIN, KW_END = "# BEGIN PANEL KEYWORDS", "# END PANEL KEYWORDS"


def patch_keywords(check=False):
    """Keep the Arduino IDE's keyword list in step with the catalogue.

    The panel classes are the names a sketch actually types, so leaving them
    out of keywords.txt is the one place the omission shows.
    """
    p = REPO / "keywords.txt"
    s = p.read_text()
    i, j = s.find(KW_BEGIN), s.find(KW_END)
    if i < 0 or j < 0:
        return ["keywords.txt: no PANEL KEYWORDS markers"]
    names = sorted("TinyGFXPanel" + name(e)
                   for entries in (PAGED, DCS) for e in entries)
    body = "\n".join(f"{n}\tKEYWORD1" for n in names)
    want = s[:i + len(KW_BEGIN)] + "\n" + body + "\n" + s[j:]
    if want == s:
        return []
    if not check:
        p.write_text(want)
    return ["keywords.txt"]


def patch_readmes(check=False):
    """Rewrite the table between the markers. Returns the files that differ."""
    differ = []
    for f, ja in (("README.md", False), ("README.ja.md", True)):
        p = REPO / f
        s = p.read_text()
        i, j = s.find(BEGIN), s.find(END)
        if i < 0 or j < 0:
            differ.append(f"{f}: no PANEL TABLE markers")
            continue
        want = s[:i + len(BEGIN)] + "\n" + readme_table(ja) + "\n" + s[j:]
        if want != s:
            differ.append(f)
            if not check:
                p.write_text(want)
    return differ


def build():
    return {name(e) + ".h": render(e, paged)
            for entries, paged in ((PAGED, True), (DCS, False))
            for e in entries}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true",
                    help="report what is out of date instead of writing")
    a = ap.parse_args()

    want = build()
    OUT.mkdir(parents=True, exist_ok=True)
    have = {p.name for p in OUT.glob("*.h")}

    stale = sorted(have - set(want))
    diff = sorted(f for f, body in want.items()
                  if not (OUT / f).exists() or (OUT / f).read_text() != body)

    readmes = patch_readmes(check=a.check) + patch_keywords(check=a.check)

    if a.check:
        if diff or stale or readmes:
            for f in diff:
                print(f"out of date: {f}")
            for f in stale:
                print(f"not in the catalogue: {f}")
            for f in readmes:
                print(f"out of date: {f}")
            sys.exit(f"{len(diff) + len(stale) + len(readmes)} file(s) differ. "
                     "Run tools/gen_panels.py")
        print(f"{len(want)} panel(s), all up to date")
        return

    for f, body in want.items():
        (OUT / f).write_text(body)
    for f in stale:
        (OUT / f).unlink()
    print(f"wrote {len(want)} panel(s) into {OUT.relative_to(REPO)}"
          + (f", removed {len(stale)}" if stale else "")
          + (f", updated {', '.join(readmes)}" if readmes else ""))


if __name__ == "__main__":
    main()
