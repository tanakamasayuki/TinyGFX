"""pushImage: placement, cropping, and the transparent form."""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
RED, GREEN, BLUE = tc.RED, tc.GREEN, tc.BLUE
WHITE, BLACK = tc.WHITE, tc.BLACK


def test_image(dut):
    dut.expect("TEST start image", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- how much was sent --------------------------------------------------
    assert r["plain_pixels"] == 16, f"a 4x4 sent {r['plain_pixels']} pixels"
    assert r["topleft_pixels"] == 4, (
        f"a 4x4 half off the top left sent {r['topleft_pixels']} pixels")
    assert r["bottomright_pixels"] == 4, (
        f"a 4x4 half off the bottom right sent {r['bottomright_pixels']} pixels")
    assert r["clipped_pixels"] == 4, f"a clipped 4x4 sent {r['clipped_pixels']} pixels"
    assert r["transparent_pixels"] == 12, (
        f"a 4x4 minus 4 red pixels sent {r['transparent_pixels']} (should be 12)")
    assert r["offscreen_pixels"] == 0, "a pushImage entirely off screen sent pixels"
    assert r["zero_pixels"] == 0, "a pushImage of width 0 sent pixels"

    # --- placement ----------------------------------------------------------
    p = tc.image(SKETCH, "plain").load()
    assert p[2, 2] == RED and p[5, 2] == GREEN, "the top row is in the wrong order"
    assert p[2, 5] == BLUE and p[5, 5] == WHITE, "the bottom row is in the wrong order"
    assert p[1, 2] == BLACK and p[6, 2] == BLACK, "it spilled left or right"

    # Off the top left, what survives is the image's bottom right 2x2
    p = tc.image(SKETCH, "topleft").load()
    assert p[0, 0] == WHITE, f"the top-left crop is off: {p[0, 0]}"
    assert p[2, 0] == BLACK, "extra pixels survived the crop"

    # Off the bottom right, what survives is the image's top left 2x2
    p = tc.image(SKETCH, "bottomright").load()
    assert p[14, 14] == RED, f"the bottom-right crop is off: {p[14, 14]}"

    # Clip (4,4,4,4) against image (2,2,4,4) overlaps in the 2x2 at (4,4)-(5,5)
    p = tc.image(SKETCH, "clipped").load()
    assert p[4, 4] == WHITE, f"wrong colour inside the clip: {p[4, 4]}"
    assert p[3, 4] == BLACK and p[6, 4] == BLACK, "it escaped the clip"

    # Transparent: only the four red pixels stay as background
    p = tc.image(SKETCH, "transparent").load()
    assert p[2, 2] == BLACK and p[3, 3] == BLACK, "the transparent colour was drawn"
    assert p[4, 2] == GREEN and p[2, 4] == BLUE, "something other than the transparent colour is missing"

    # --- 1bpp bitmaps -------------------------------------------------------
    #
    # **An invariant.** Throwing a fillRect per run must not differ by a pixel
    # from placing each pixel with drawPixel. How runs get coalesced is a
    # question of speed; the picture may not change.
    assert r["bmp5_diff"] == 0, f"width 5 (with padding) differs by {r['bmp5_diff']} pixels"
    assert r["bmp8_diff"] == 0, f"width 8 differs by {r['bmp8_diff']} pixels"

    # Coalescing means fewer calls, but the same pixels. A different pixel
    # count would mean drawing too much or too little.
    assert r["bmp5_run_pixels"] == r["bmp5_px_pixels"], (
        f"different pixel counts: runs {r['bmp5_run_pixels']} / "
        f"per pixel {r['bmp5_px_pixels']}")

    # A 0 bit is left alone (transparent). The set bits in bmp8x3 are
    # 4 from 0xA5, 0 from 0x00 and 8 from 0xFF: 12 in all.
    lit = bin(0xA5).count("1") + bin(0x00).count("1") + bin(0xFF).count("1")
    assert r["bmp_kept_bg"] == 16 * 16 - lit, (
        f"{r['bmp_kept_bg']} pixels left as background (want {16 * 16 - lit}; "
        "transparency is not working)")

    # Extreme coordinates. If the clipping arithmetic overflows int16_t, it
    # reads in front of the source image. `pushImage` keeps its far edges in
    # int32_t so it is safe - but **safe by argument only**, so pin it with a
    # number.
    assert r["extreme_pixels"] == 0, (
        f"entirely off screen, yet {r['extreme_pixels']} pixels were sent")

    # --- byte swapping ------------------------------------------------------
    #
    # TinyGFX has no setSwapBytes() (DECISIONS.ja.md D29). A runtime mode cost
    # "+44 B and +4 B of RAM even to sketches that never swap", so it is a free
    # function you pay for when you call it. **0 B unless called** (measured).
    assert r["swapped_diff"] > 0, (
        "swapping the byte order changed nothing; the swap is not working")
    assert r["swap_roundtrip"] == 16, (
        f"only {r['swap_roundtrip']} of 16 words came back after swapping twice")
    assert r["swap_zero_kept"] == 1, "a length of 0 still wrote to the array"
