"""The `_SeqCom` entry: **same driver, same size, different value.**

A 128x64 SSD1306 is sold with its COM lines wired both ways, so the catalogue
carries two entries at that size. **The size cannot tell them apart** - which is
precisely what a derivation from the height gets wrong, and why the height is
not used for this (docs/PANEL_TUNING.ja.md 3).

Only one panel per driver fits in a sketch, so this one stands alone.
`tests/panels/` drives a 128x32; between them they show the value comes from
the header rather than from the dimensions.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent

ALTERNATIVE = 0x12  # the datasheet reset, and what plain SSD1306_128x64 sends
SEQUENTIAL = 0x02   # what this entry exists to send instead


def test_panels_seqcom(dut):
    dut.expect("TEST start panels_seqcom", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # Same size as the default 128x64 entry - that is the whole point.
    assert (r["width"], r["height"]) == (128, 64)
    assert r["buffer_bytes"] == 128 * 64 // 8
    assert r["mux"] == 0x3F, (
        f"multiplex is {r['mux']:#04x}; derived from a height of 64 it is 0x3F. "
        "A different value here would mean this is not really the same size")

    # ★ and yet a different COM pin layout.
    assert r["com_pins"] == SEQUENTIAL, (
        f"COM pins came out {r['com_pins']:#04x}. {ALTERNATIVE:#04x} is what the "
        "plain SSD1306_128x64 sends, so this entry carried nothing")
