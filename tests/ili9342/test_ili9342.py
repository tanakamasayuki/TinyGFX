"""How an ILI9342C assembles MADCTL, colour order and mirroring.

The ILI9342's GRAM is landscape from the start, so rotation 0 is 320x240 - its
native size. The table itself is the ST7789's; the only differences are the BGR
bit and the XOR from setMirror.

**Whether that table is right on real glass cannot be known here**
(docs/MANUAL_TEST.ja.md M0). What this holds is that the implementation follows
the table. If the hardware disagrees, fix the table and these expectations
together.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent

MY, MX, MV, BGR = 0x80, 0x40, 0x20, 0x08
RED565 = 0xF800

# The bare table, colour order removed (the same as the ST7789)
BASE = {
    0: 0,
    1: MV | MX,
    2: MX | MY,
    3: MV | MY,
}
SIZE = {0: (32, 16), 1: (16, 32), 2: (32, 16), 3: (16, 32)}


def test_ili9342(dut):
    dut.expect("TEST start ili9342", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    problems = []
    for rot, base in BASE.items():
        w, h = SIZE[rot]
        checks = {
            f"bgr{rot}_madctl": base | BGR,
            f"rgb{rot}_madctl": base,
            f"flip{rot}_madctl": (base ^ (MX | MY)) | BGR,
            f"bgr{rot}_w": w,
            f"bgr{rot}_h": h,
            # No ILI9342 module has an origin offset; the window passes through
            f"bgr{rot}_xs": 0,
            f"bgr{rot}_ys": 0,
        }
        for key, want in checks.items():
            got = r[key]
            if got != want:
                problems.append(f"{key}: {got:#04x} != {want:#04x}"
                                if key.endswith("madctl") else f"{key}: {got} != {want}")
    assert not problems, "differs from the ILI9342 table: " + "; ".join(problems)

    madctls = {r[f"bgr{rot}_madctl"] for rot in range(4)}
    assert len(madctls) == 4, f"duplicate MADCTL values: {sorted(madctls)}"

    # Mirroring both axes is the same as rotating 180 degrees, and shows up as
    # a swap in the table. (Mirroring one axis cannot be made from a rotation -
    # which is why setMirror exists at all.)
    assert r["flip0_madctl"] == r["bgr2_madctl"], "both-axis mirror does not match rotation 2"
    assert r["flip2_madctl"] == r["bgr0_madctl"], "both-axis mirror does not match rotation 0"

    # Does what was drawn reach the GRAM (i.e. does the ST7789 command stream
    # work here too)?
    assert r["hit"] == RED565, f"inside the fill is {r['hit']:#06x}"
    assert r["edge"] == RED565, f"the bottom-right of the fill is {r['edge']:#06x}"
    assert r["miss"] == 0, f"unpainted, to the left: {r['miss']:#06x}"
    assert r["past"] == 0, f"unpainted, past the bottom right: {r['past']:#06x}"
