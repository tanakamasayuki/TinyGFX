"""Wrapping at the right edge (Adafruit_GFX's setTextWrap).

**Not on by default.** Wrapping correctly means knowing whether a character
fits **before** drawing it, and that is a second entry point into the font
decoder (`advance`, on top of `draw`). The linker cannot drop it once `write()`
refers to it, so **every sketch that prints pays 164 B** (measured on a
CH32V003) - the same reason setTextDatum is not here either.

It appears only when `TINYGFX_TEXT_WRAP` is 1, and this test exercises that
side. The 0 side is measured to be byte-for-byte what it was before wrapping
existed (docs/DECISIONS.ja.md D33).

The font is `tgfx_utf8`. **Digits advance by 4 and '℃' by 8** - a wrap decision
made against a fixed width fails here.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent


def test_textwrap(dut):
    dut.expect("TEST start textwrap", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    W = r["width"]
    line = r["line"]
    adv = r["adv_digit"]
    wide = r["adv_wide"]
    assert W == 32 and adv == 4 and wide == 8, (
        f"the premises have moved: width {W} / digit advance {adv} / '℃' {wide}")

    per_line = W // adv          # 8 characters
    assert per_line == 8

    # --- off: no wrapping ---------------------------------------------------
    # The pen moves 12 characters and the line never changes. What falls off
    # the screen is the panel's business to clip.
    assert r["off_x"] == 12 * adv, f"wrapping off, yet x={r['off_x']}"
    assert r["off_y"] == 0, f"wrapping off, yet it moved to a new line (y={r['off_y']})"

    # --- on: wraps after 8 characters ---------------------------------------
    assert r["on_y"] == line, (
        f"it should wrap exactly once, but y={r['on_y']} (line height {line})")
    assert r["on_x"] == (12 - per_line) * adv, (
        f"x after wrapping is {r['on_x']}; want the remaining {12 - per_line} characters")
    # Two lines now, so more rows carry ink than before
    assert r["on_rows"] > r["off_rows"], (
        f"wrapping added no rows: {r['on_rows']} against {r['off_rows']}")
    assert r["on_right"] < W, f"drawn past the screen width (x={r['on_right']})"

    # --- a wrapped line restarts at the x of setCursor ----------------------
    # Starting at x=4 fits 7 characters (4 + 7*4 = 32); the 8th wraps.
    fit = (W - 4) // adv
    assert fit == 7
    assert r["indent_y"] == line, f"it should wrap exactly once, but y={r['indent_y']}"
    assert r["indent_x"] == 4 + (12 - fit) * adv, (
        f"wrapped to x={r['indent_x']}; it should return to the setCursor x of 4")

    # --- a character of a different width -----------------------------------
    # Seven digits put the pen at x=28. '℃' advances 8, and 28+8=36 > 32, so it
    # does not fit. **A wrap decision made against a fixed advance fails here.**
    assert r["wide_y"] == line, (
        f"the wide character does not fit, yet nothing wrapped (y={r['wide_y']})")
    assert r["wide_x"] == wide, (
        f"x after wrapping is {r['wide_x']}; want the {wide} of one '℃'")
    assert r["wide_right"] < W, f"drawn past the screen width (x={r['wide_right']})"

    # --- a newline is still a newline ---------------------------------------
    assert r["newline_y"] == line, f"the newline moved y to {r['newline_y']}"
    assert r["newline_x"] == 4 + adv, (
        f"x after the newline is {r['newline_x']}; want the setCursor x of 4 plus one character")
