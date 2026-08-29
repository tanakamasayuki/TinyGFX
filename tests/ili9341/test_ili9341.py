"""ILI9341 - **the one panel that had no test** (added 2026-08-29).

The ILI9341 is 21 lines, almost all of it inherited from `PanelDcs`. Only three
things separate it from an ILI9342C:

1. a portrait GRAM (240x320)
2. **inversion off** (an ILI9342C and an ST7789 have it on)
3. **mirrored on X** (an ordinary breakout wants MADCTL 0x48 at rotation 0)

All three show up in the command stream `init()` emits.
**And nothing was watching that stream.** `TinyGFXBusCapture` silently discards
commands it does not know, so dropping SLPOUT or changing COLMOD left every host
test passing.

So this test holds two things:

- **A.** that an ILI9341 is an ILI9341 (the three points above)
- **B.** **the init sequence of all three DCS panels.** All three are run on the
  same bus, pinning that only one byte - the inversion - differs

**Whether that MADCTL is right on real glass cannot be known here**
(docs/MANUAL_TEST.ja.md M5). What is held is that the implementation follows the
table. If the hardware disagrees, fix the table and these expectations together.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent

MY, MX, MV, BGR = 0x80, 0x40, 0x20, 0x08
RED565 = 0xF800

SWRESET, SLPOUT, COLMOD, INVOFF, INVON, NORON, MADCTL, DISPON = (
    0x01, 0x11, 0x3A, 0x20, 0x21, 0x13, 0x36, 0x29)

# The bare table, colour order and mirroring removed (shared with ST7789 / ILI9342)
BASE = {0: 0, 1: MV | MX, 2: MX | MY, 3: MV | MY}


def seq(r, name):
    """Pull <name>'s init sequence out of the report as (command, first arg)."""
    n = r[f"{name}_len"]
    return [(r[f"{name}_cmd{i}"], r[f"{name}_arg{i}"]) for i in range(n)]


def test_ili9341(dut):
    dut.expect("TEST start ili9341", timeout=20)
    dut.expect("TEST done", timeout=120)

    r = tc.report(SKETCH)

    # --- B. the init sequence -----------------------------------------------
    #
    # The order matters. Sending COLMOD before SLPOUT, or DISPON first, leaves
    # every pixel check passing (BusCapture only looks at 0x2A/0x2B/0x2C).
    # **That is a defect only hardware would reveal, so pin the order here.**
    def expect_seq(invert_cmd, madctl):
        return [
            (SWRESET, 0x00),
            (SLPOUT, 0x00),
            (COLMOD, 0x55),      # 16bit/pixel
            (invert_cmd, 0x00),
            (NORON, 0x00),
            (MADCTL, madctl),    # setRotation(0) runs here
            (DISPON, 0x00),
        ]

    cases = {
        # panel        inversion  MADCTL at rotation 0
        "st7789":  (INVON,  BASE[0]),                  # RGB, no mirror
        "ili9341": (INVOFF, (BASE[0] ^ MX) | BGR),     # **inversion off, X mirror, BGR**
        "ili9342": (INVON,  BASE[0] | BGR),            # BGR, no mirror
    }
    for name, (inv, madctl) in cases.items():
        got = seq(r, name)
        want = expect_seq(inv, madctl)
        assert got == want, (
            f"{name}'s init sequence differs:\n"
            f"  got  {[(hex(c), hex(a)) for c, a in got]}\n"
            f"  want {[(hex(c), hex(a)) for c, a in want]}")

    # The three differ **only in the inversion and the MADCTL**. Anything else
    # diverging means the shared base has come apart.
    st, i41, i42 = seq(r, "st7789"), seq(r, "ili9341"), seq(r, "ili9342")
    differing = [i for i in range(len(st)) if len({st[i], i41[i], i42[i]}) != 1]
    assert differing == [3, 5], (
        f"the three panels differ at {differing}; only inversion (3) and MADCTL (5) should")

    # Only the ILI9341 has inversion off (get this wrong and the colours invert
    # on real glass)
    assert i41[3][0] == INVOFF, "the ILI9341 is sending INVON"
    assert st[3][0] == INVON and i42[3][0] == INVON

    # --- A. it is an ILI9341 -------------------------------------------------
    # The default GRAM is portrait, 240x320, swapping at rotations 1 and 3.
    SIZE = {0: (240, 320), 1: (320, 240), 2: (240, 320), 3: (320, 240)}

    problems = []
    for rot, base in BASE.items():
        w, h = SIZE[rot]
        checks = {
            # including the X mirror the implementation carries
            f"def{rot}_madctl": (base ^ MX) | BGR,
            # taking the mirror off leaves the bare table plus BGR
            f"nomir{rot}_madctl": base | BGR,
            # turning the colour order off drops the BGR bit, keeping the mirror
            f"rgb{rot}_madctl": base ^ MX,
            f"def{rot}_w": w,
            f"def{rot}_h": h,
            # No ILI9341 module has an origin offset; the window passes through
            f"def{rot}_xs": 0,
            f"def{rot}_ys": 0,
        }
        for key, want in checks.items():
            got = r[key]
            if got != want:
                problems.append(f"{key}: {got:#04x} != {want:#04x}"
                                if key.endswith("madctl") else f"{key}: {got} != {want}")
    assert not problems, "differs from the ILI9341 table: " + "; ".join(problems)

    # **Rotation 0 is 0x48.** That is the value every other library sends to
    # this part, and the number the header comment rests on.
    assert r["def0_madctl"] == 0x48, (
        f"MADCTL at rotation 0 is {r['def0_madctl']:#04x}; an ordinary breakout wants 0x48")

    madctls = {r[f"def{rot}_madctl"] for rot in range(4)}
    assert len(madctls) == 4, f"duplicate MADCTL values: {sorted(madctls)}"

    # It must not have become the ILI9342's table (catches a mix-up): at
    # rotation 0 the ILI9341 sets MX and the ILI9342 does not.
    assert r["def0_madctl"] != (BASE[0] | BGR), "this is the ILI9342 table"

    # --- does what was drawn reach the GRAM ---------------------------------
    assert r["hit"] == RED565, f"inside the fill is {r['hit']:#06x}"
    assert r["edge"] == RED565, f"the bottom-right of the fill is {r['edge']:#06x}"
    assert r["miss"] == 0, f"unpainted, to the left: {r['miss']:#06x}"
    assert r["past"] == 0, f"unpainted, past the bottom right: {r['past']:#06x}"
