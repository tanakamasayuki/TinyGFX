"""A monochrome OLED on I2C (SSD1306), end to end.

The host's Wire probe picks up the bytes the real `TinyGFXBusI2C` actually
sent, and a model of an SSD1306 puts them back together. The core knows about
neither SSD1306 nor I2C.

The point is what differs from a colour panel on SPI:
  - it owns a framebuffer (RGB565 reduced to 1bpp as "lit unless zero")
  - nothing is transferred until `display()` is called
  - only the pages that changed go out
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
W, H = 128, 64
PAGE_BYTES = W  # one page = 128 bytes across, 8 pixels tall
WHITE, BLACK = tc.WHITE, tc.BLACK


def test_i2c(dut):
    dut.expect("TEST start i2c", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- initialisation goes out as transactions ----------------------------
    assert r["init_transactions"] >= 20, (
        f"init sequence too short: {r['init_transactions']} transactions")

    # --- not one byte moves until display() ---------------------------------
    assert r["bytes_before_display"] == 0, (
        f"{r['bytes_before_display']} bytes went out before display()")
    assert r["bytes_full"] == PAGE_BYTES * 8, (
        f"a full transfer was {r['bytes_full']} bytes (should be {PAGE_BYTES * 8})")

    # --- only the pages that changed go out ---------------------------------
    assert r["bytes_one_page"] == PAGE_BYTES, (
        f"a one-page change sent {r['bytes_one_page']} bytes "
        f"(should be {PAGE_BYTES}); dirty tracking is not working")

    # --- rotation -----------------------------------------------------------
    assert (r["rot1_w"], r["rot1_h"]) == (H, W), (
        f"rotation 1 gave {r['rot1_w']}x{r['rot1_h']} (should be {H}x{W})")

    # --- the picture --------------------------------------------------------
    full = tc.image(SKETCH, "full")
    assert tc.colors(full) == {WHITE: W * H}, (
        f"the screen fill is not one colour: {tc.colors(full)}")

    one = tc.image(SKETCH, "onepage").load()
    assert one[8, 8] == WHITE and one[39, 15] == WHITE, "the rectangle is missing"
    assert one[7, 8] == BLACK and one[40, 8] == BLACK, "the rectangle spilled sideways"
    assert one[8, 7] == BLACK and one[8, 16] == BLACK, "the rectangle spilled vertically"

    scene = tc.image(SKETCH, "scene").load()
    assert scene[0, 0] == WHITE and scene[W - 1, H - 1] == WHITE, "a corner of the frame is missing"
    assert scene[96, 32] == WHITE, "the centre of the circle is not painted"
    assert scene[96, 32 - 13] == BLACK, "the circle is bigger than its radius"
    assert scene[64, 40] == BLACK, "somewhere that should be empty was painted"

    # At rotation 1, (0,0)-(7,23) is x=0..23 / y=56..63 in panel coordinates
    rot = tc.image(SKETCH, "rot1").load()
    assert rot[0, 63] == WHITE and rot[23, 56] == WHITE, "the rotated rectangle is in the wrong place"
    assert rot[24, 63] == BLACK and rot[0, 55] == BLACK, "the rotated rectangle spilled"

    # The fast path (byte-wise filling) must match drawing pixel by pixel:
    # six rectangles across four rotations. **Faster, and not one bit different.**
    assert r["fillrect_fastpath_diff"] == 0, (
        f"the fillRect fast path differs by {r['fillrect_fastpath_diff']} bytes")

    # --- dimensions and the buffer contract for a paged panel (P1 of the
    # 2026-08-29 review) ------------------------------------------------------
    #
    # The init sequence was hard-coded for 128x64 while the constructor took any
    # w / h. Sending a 64-row multiplex ratio (0x3F) to a 128x32 squashes the
    # picture into half the glass. Both are now **derived from the height**, so
    # pin that here.
    assert r["mux64"] == 0x3F, f"the 128x64 multiplex ratio is {r['mux64']:#04x}"
    assert r["com64"] == 0x12, f"the 128x64 COM pin config is {r['com64']:#04x}"
    assert r["mux32"] == 0x1F, (
        f"the 128x32 multiplex ratio is {r['mux32']:#04x} (still the 64-row value?)")
    assert r["com32"] == 0x02, (
        f"the 128x32 COM pin config is {r['com32']:#04x} (still the 64-row value?)")

    # --- the begin() contract -----------------------------------------------
    #
    # **The return value means "is this configuration usable"**, not "is there
    # really a panel on the other end" - which a write-only panel cannot tell.
    # A null buffer, a height that is not a multiple of 8, a zero width: none of
    # those are visible to the compiler, so they are caught here.
    assert r["begin_ok"] == 1, "begin() returned false for a sound configuration"
    assert r["begin_null_buffer"] == 0, "begin() returned true with a null buffer"
    assert r["begin_odd_height"] == 0, "begin() returned true for a height not a multiple of 8"
    assert r["begin_zero_width"] == 0, "begin() returned true for a width of 0"
