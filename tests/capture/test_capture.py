"""BusCapture rebuilds a picture from an ST7789 command stream. Tier 1 rests on
this one.

Nothing else in the drawing tests can be written until this passes.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
BLUE, RED, GREEN, BLACK = tc.BLUE, tc.RED, tc.GREEN, tc.BLACK


def test_capture(dut):
    dut.expect("TEST start capture", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    assert r["init_commands"] > 5, f"init sequence too short: {r['init_commands']}"

    # --- exactly the right number of pixels, no more, no fewer --------------
    assert r["fillscreen_pixels"] == 64 * 64, f"fillScreen sent {r['fillscreen_pixels']} pixels"
    assert r["fillrect_pixels"] == 8 * 8, f"fillRect(8x8) sent {r['fillrect_pixels']} pixels"
    assert r["pixel_pixels"] == 1, f"drawPixel sent {r['pixel_pixels']} pixels"

    # --- the window values --------------------------------------------------
    assert (r["win_xs"], r["win_ys"]) == (3, 5)
    assert (r["win_xe"], r["win_ye"]) == (12, 11)  # x+w-1, y+h-1
    assert (r["off_xs"], r["off_ys"]) == (2, 1), "the origin offset is not reaching the window"

    assert r["txn_depth"] == 0, "startWrite / endWrite are not balanced"

    # --- the rebuilt picture ------------------------------------------------
    img = tc.image(SKETCH, "fillscreen")
    assert img.size == (64, 64)
    assert tc.colors(img) == {BLUE: 64 * 64}, f"the screen is not solid blue: {tc.colors(img)}"

    img = tc.image(SKETCH, "fillrect")
    assert img.getpixel((4, 4)) == RED and img.getpixel((11, 11)) == RED
    for p in [(3, 4), (12, 11), (4, 3), (4, 12)]:
        assert img.getpixel(p) == BLACK, f"the rectangle spilled onto {p}"
    assert tc.colors(img)[RED] == 64

    img = tc.image(SKETCH, "pixel")
    assert img.getpixel((9, 3)) == GREEN
    assert tc.colors(img)[GREEN] == 1
