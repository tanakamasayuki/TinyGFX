"""**A CellFont from the real generator (the LGFXFontToolJs CLI) must render.**

Not an interim header - the shape that actually ships, fed in unchanged. This
one test walks into three of the awkward parts of the spec at once.

- variable pitch (a glyph table; `width` / `xAdvance` / `bytesPerGlyph` are 0)
- a sparse index with a head block (`headCount=2`, `first=0x32`)
- **codes below `first` living in the tail** (0x20 / 0x2E) - the trap in spec 7.1
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent


def test_cli_font_renders(dut):
    dut.expect("TEST start clifont", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # Must agree with what the generator wrote in the header comment
    # (line box 11px, ascent 10)
    assert r["line"] == 11, f"line height is {r['line']}"
    assert r["ascent"] == 10, f"ascent is {r['ascent']}"

    # Twelve characters worth of advance: full-width 12 and half-width 6 mixed
    assert r["width"] > 0, "the pen never moved"
    assert r["lit"] > 200, f"too few pixels drawn: {r['lit']}"

    # An uncovered code draws nothing and advances nothing (this font has no tofu)
    assert r["missing_adv"] == 0, f"an uncovered code advanced by {r['missing_adv']}"
    assert r["missing_lit"] == 0, f"an uncovered code drew {r['missing_lit']} pixels"

    im = tc.image(SKETCH, "clifont")
    assert im.size == (128, 16)
