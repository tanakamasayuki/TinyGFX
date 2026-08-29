"""The SH1106 wiring, checked without hardware.

An SH1106 differs from an SSD1306 in exactly two ways, and the host's Wire
probe can see both.

- **132 columns of RAM behind 128 columns of glass.** The left edge of the
  picture is RAM column 2
- **no column/page range commands (0x21 / 0x22).** The cursor is placed per
  page and one page is streamed at a time

Everything shared (`PanelPaged`) is the same code the SSD1306 uses, so drawing
**the same picture on both, decoding it, and requiring not one bit of
difference** isolates the transfer layer on its own.

**Unconfirmed on real glass** (docs/MANUAL_TEST.ja.md M6). What this holds is
that the bytes TinyGFX emits as an SH1106, read the way the SH1106 datasheet
says to read them, make the right picture. Whether a real SH1106 agrees with
that reading can only be found out on hardware.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
W, H = 128, 64


def test_sh1106(dut):
    dut.expect("TEST start sh1106", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- traffic: 128 bytes a page ------------------------------------------
    assert r["sh_bytes"] == W * H // 8, f"wrong amount of traffic: {r['sh_bytes']}"
    assert r["ssd_bytes"] == W * H // 8, f"wrong amount of traffic on the SSD1306 side: {r['ssd_bytes']}"

    # --- nothing written outside the glass ----------------------------------
    # Columns 0, 1 and 130, 131 are the parts of the 132-column RAM nobody sees.
    assert r["outside_glass"] == 0, (
        f"{r['outside_glass']} bytes written outside the glass "
        "(the column offset is wrong)")

    # --- **the point.** The same picture an SSD1306 gives --------------------
    assert r["sh_vs_ssd_diff"] == 0, (
        f"SH1106 and SSD1306 differ by {r['sh_vs_ssd_diff']} bytes")

    # --- the picture is not one flat colour ---------------------------------
    # All-white matching all-white would prove nothing.
    assert 0 < r["sh_lit"] < W * H, (
        f"the picture is one flat colour ({r['sh_lit']} lit of {W * H})")

    # --- the offset is actually wired up ------------------------------------
    # With the offset at 0 it writes from RAM column 0, so the picture taken
    # from column 0 should match the SSD1306. A 0 here means setColumnOffset()
    # is not connected to anything.
    assert r["offset0_matches_ssd"] == 1, "setColumnOffset() has no effect"

    # --- the fast path for vertically packed bitmaps ------------------------
    #
    # **A page-aligned vertical bitmap is this panel's buffer, exactly.**
    # `pushVBitmap()` blits it. Pinning it against the general `drawImage()`
    # keeps it a faster path rather than a different one.
    #
    # When it is not aligned it must **draw nothing and return false**. Letting
    # the caller fall back to the general path beats silently drawing in the
    # wrong place.
    assert r["vblit_taken"] == 1, "aligned, yet the fast path was not taken"
    assert 0 < r["vblit_lit"] < 128 * 64, (
        f"the picture is one flat colour ({r['vblit_lit']} lit)")
    assert r["vblit_diff"] == 0, (
        f"the fast and general paths differ by {r['vblit_diff']} bytes")

    assert r["vblit_unaligned"] == 0, "accepted a bitmap that is not page aligned"
    assert r["vblit_offpanel"] == 0, "accepted a bitmap hanging off the panel"
    assert r["vblit_rotated"] == 0, "accepted a bitmap while rotated"
