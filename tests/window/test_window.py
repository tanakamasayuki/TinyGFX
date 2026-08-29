"""How rotation and offset come out in MADCTL and CASET / RASET.

Rotation is done by the controller's MADCTL by design (docs/DECISIONS.ja.md
D7), so what can be checked in software is three things: that width and height
swap, that MADCTL takes the right value, and that the offset reaches the window.

**Whether that MADCTL value is right on real glass cannot be known here**
(MANUAL_TEST M2). What this holds is that the implementation follows the table
below. If the table is wrong, the hardware will say so - and then this table
gets fixed too.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent

MADCTL_MY, MADCTL_MX, MADCTL_MV = 0x80, 0x40, 0x20

# A 135x240 panel on a 240x320 GRAM, offset (52, 40) at rotation 0
#   cs2 = 240 - 135 - 52 = 53
#   rs2 = 320 - 240 - 40 = 40
EXPECTED = {
    0: dict(madctl=0x00, w=135, h=240, xs=52, ys=40),
    1: dict(madctl=MADCTL_MV | MADCTL_MX, w=240, h=135, xs=40, ys=53),
    2: dict(madctl=MADCTL_MX | MADCTL_MY, w=135, h=240, xs=53, ys=40),
    3: dict(madctl=MADCTL_MV | MADCTL_MY, w=240, h=135, xs=40, ys=52),
}


def test_window(dut):
    dut.expect("TEST start window", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    problems = []
    for rot, want in EXPECTED.items():
        for key, value in want.items():
            got = r[f"rot{rot}_{key}"]
            if got != value:
                problems.append(f"r={rot} {key}: {got} != {value}")
    assert not problems, "values per rotation differ from the table: " + "; ".join(problems)

    madctls = {r[f"rot{rot}_madctl"] for rot in range(4)}
    assert len(madctls) == 4, f"duplicate MADCTL values: {sorted(madctls)}"

    for rot in range(4):
        xs, ys = r[f"zero{rot}_xs"], r[f"zero{rot}_ys"]
        assert (xs, ys) == (0, 0), f"a panel with no offset gave ({xs},{ys}) at r={rot}"

    assert (r["clip_w"], r["clip_h"]) == (240, 135), "setRotation did not swap width/height"

    # --- the setters must not depend on call order (P0 of the 2026-08-29
    # review) ----------------------------------------------------------------
    #
    # setGramSize() and setOffset() must derive the offset for the current
    # rotation on the spot: either order, before or after begin(). They used to
    # be derived only inside setRotation(), so a sketch that never rotated
    # never got its offset at all.
    for who, label in (("late", "after begin()"),
                       ("swap", "after begin(), reversed order"),
                       ("early", "before begin()")):
        got = (r[f"{who}_xs"], r[f"{who}_ys"])
        assert got == (52, 40), (
            f"setGramSize/setOffset called {label} left the window at {got} "
            "(it only takes effect once setRotation runs)")
