"""The panel catalogue: does a panel header carry what its module needs?

A panel is a preset - the product you bought - and what it carries is only what
cannot be derived. **The driver works everything else out**, so what is worth
checking is the two things a preset really decides:

1. the COM pin layout, a fact about how the glass is wired
2. the column offset, when the glass does not sit centred in RAM

The sketch drives a **128x32**, because that is the one whose COM pin layout
differs from the datasheet reset. If the preset mechanism carried nothing, this
is the test that would notice.

Only one panel per driver can exist in a sketch (docs/GLOSSARY.md 3), so a
second driver - an ST7789 - is included alongside to show the guard is per
driver and not global.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent


def test_panels(dut):
    dut.expect("TEST start panels", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- the sketch does not have to know the size ---------------------------
    assert (r["panel_w"], r["panel_h"]) == (128, 32)
    assert r["buffer_bytes"] == 128 * 32 // 8, (
        f"kBufferBytes is {r['buffer_bytes']}; a wrong one is a silent overrun")
    assert (r["width"], r["height"]) == (128, 32)

    # --- derived, so the preset does not carry it ----------------------------
    assert r["mux"] == 0x1F, (
        f"multiplex is {r['mux']:#04x}; it is derived as height - 1 = 0x1F")
    assert r["col0"] == 0, (
        f"column offset is {r['col0']}; a 128-wide glass on 128 columns of RAM "
        "is not offset at all")

    # --- ★ carried by the preset --------------------------------------------
    #
    # This glass wires its COM lines sequentially. The datasheet reset value is
    # alternative (0x12), so a bare driver sends that - **only the panel header
    # knows otherwise.** If this reads 0x12, the preset reached nothing.
    assert r["com_pins"] == 0x02, (
        f"COM pins came out {r['com_pins']:#04x}. 0x12 means the panel header's "
        "value never reached the driver")

    # --- a panel of another driver coexists ----------------------------------
    assert (r["tft_w"], r["tft_h"]) == (240, 240)

    # A 240x240 ST7789 has 240x320 of memory, so rotation 2 starts 80 rows down.
    # **A driver that was never told the GRAM size starts at 0** - which is what
    # every example in this repository did until the panel headers arrived.
    assert r["tft_rot2_ys"] == 80, (
        f"rotation 2 starts at row {r['tft_rot2_ys']}; a 240x240 ST7789 sits 80 "
        "rows down its 240x320 memory. 0 means the GRAM size never got set")
