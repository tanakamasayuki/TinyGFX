"""Text drawing.

The fonts come **from the real generator (the LGFXFontToolJs CLI)** and live in
tgfx_font: the ten digits of `lgfxJapanGothic_8`, 4x6 of ink, advance 4, line
box 9.

The same ten characters are there encoded three different ways, and the last
check is that **all three draw the same picture** - which encoding gets used is
the generator's business and must not be visible from the sketch.
"""

from pathlib import Path

from PIL import ImageChops

import tgfx_check as tc

SKETCH = Path(__file__).parent
BLACK, BLUE = tc.BLACK, tc.BLUE
ADV, LINE = 4, 9


def test_text(dut):
    dut.expect("TEST start text", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    assert r["font_height"] == LINE, f"fontHeight={r['font_height']}"
    assert r["text_width"] == 5 * ADV, f"textWidth('12345')={r['text_width']}"
    assert r["draw_width"] == r["text_width"], (
        f"drawString returned {r['draw_width']} against textWidth {r['text_width']}")
    assert r["width_x2"] == 2 * 5 * ADV, f"double-size width is {r['width_x2']}"
    assert r["height_x2"] == 2 * LINE, f"double-size height is {r['height_x2']}"

    # A character the font does not cover
    assert r["oor_advance"] == 0, "an uncovered code returned an advance"
    assert r["oor_pixels"] == 0, "an uncovered code sent pixels"

    # --- transparent: the glyph pixels and nothing else ---------------------
    plain = tc.image(SKETCH, "plain")
    on = tc.lit(plain)
    assert on, "not one pixel of text was drawn"
    xs = [x for x, _ in on]
    ys = [y for _, y in on]
    assert min(xs) >= 2 and max(xs) < 2 + 5 * ADV, f"spilled sideways: {min(xs)}..{max(xs)}"
    assert min(ys) >= 3 and max(ys) < 3 + LINE, f"spilled vertically: {min(ys)}..{max(ys)}"

    # Double size spreads over about four times the area
    dbl = tc.lit(tc.image(SKETCH, "double"))
    assert max(x for x, _ in dbl) >= 3 * ADV, "double size did not spread"

    # --- with a background colour: the cell fills ---------------------------
    opaque = tc.image(SKETCH, "opaque").load()
    holes = [(x, y)
             for y in range(3, 3 + LINE)
             for x in range(2, 2 + 5 * ADV)
             if opaque[x, y] == BLACK]
    assert not holes, f"holes in the cell despite a background: {len(holes)} px {holes[:8]}"
    assert opaque[1, 3] == BLACK, "the background painted past the left of the cell"
    assert opaque[2, 3 + LINE] == BLACK, "the background painted below the cell"

    # --- transparent leaves the background alone ----------------------------
    tr = tc.image(SKETCH, "transparent").load()
    assert tr[2, 3] == BLACK or tr[2 + 5, 3] == BLACK, (
        "the gaps in the glyph were painted despite being transparent")

    # --- the CellFont variants must draw the same picture -------------------
    # Contiguous vs sparse index, and glyph table vs none, are choices the
    # generator makes. Not one pixel may differ between them.
    ref = tc.image(SKETCH, "var_fixed")
    assert tc.lit(ref), "the baseline variant drew nothing"
    for name in ["var_records", "var_sparse"]:
        img = tc.image(SKETCH, name)
        box = ImageChops.difference(ref, img).getbbox()
        assert box is None, f"{name} differs from the fixed-pitch version: bbox={box}"

    # --- centred and right-aligned ------------------------------------------
    #
    # **Stated as an invariant.** The aligned call and a drawString placed at
    # the computed position must not differ by a pixel. Expected absolute
    # coordinates would depend on the glyph's left bearing.
    #
    # LovyanGFX also offers this through setTextDatum; TinyGFX has only the
    # explicit calls. A datum is state drawString consults every time, so
    # everyone who never centres anything would pay 204 B (measured on a
    # CH32V003; the explicit calls cost nothing until called).
    w = r["plain_width"]
    assert w > 0, "textWidth returned 0"
    assert r["center_matches"] == 1, "drawCenterString differs from drawString(cx - w/2)"
    assert r["right_matches"] == 1, "drawRightString differs from drawString(rx - w)"
    assert r["center_moved"] == 1, "centred and right-aligned landed in the same place"
    assert r["center_ret"] == w, f"drawCenterString returned {r['center_ret']} (want {w})"
    assert r["right_ret"] == w, f"drawRightString returned {r['right_ret']} (want {w})"
