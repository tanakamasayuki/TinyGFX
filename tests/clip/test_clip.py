"""The clipping invariant.

Inside the clip, not one pixel differs from drawing without a clip; outside it,
not one pixel is touched. Stating it that way needs no expected image, so
changing what is drawn does not break the test.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
BLACK = (0, 0, 0)
CX, CY, CW, CH = 10, 12, 30, 26


def test_clip(dut):
    dut.expect("TEST start clip", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)
    free = tc.image(SKETCH, "free").load()
    clipped = tc.image(SKETCH, "clipped").load()

    inside_mismatch, outside_dirty = [], []
    for y in range(64):
        for x in range(64):
            inside = CX <= x < CX + CW and CY <= y < CY + CH
            if inside:
                if free[x, y] != clipped[x, y]:
                    inside_mismatch.append((x, y))
            elif clipped[x, y] != BLACK:
                outside_dirty.append((x, y))

    assert not outside_dirty, (
        f"{len(outside_dirty)} pixels painted outside the clip: {outside_dirty[:8]}")
    assert not inside_mismatch, (
        f"inside the clip differs from no clip: {len(inside_mismatch)} pixels "
        f"{inside_mismatch[:8]}")

    # Both assertions above pass trivially on an empty picture, so check
    # something was actually drawn.
    drawn = sum(1 for y in range(CY, CY + CH) for x in range(CX, CX + CW)
                if clipped[x, y] != BLACK)
    assert drawn > CW * CH // 4, f"the clip region is nearly empty: {drawn} pixels"

    assert r["clipped_pixels"] < r["free_pixels"], (
        "clipping did not reduce the traffic "
        f"({r['clipped_pixels']} vs {r['free_pixels']})")
    assert r["oversize_clip_pixels"] == 64 * 64, (
        f"a clip larger than the screen sent {r['oversize_clip_pixels']} pixels (should be 4096)")
    assert r["empty_clip_pixels"] == 0, (
        f"an empty clip sent {r['empty_clip_pixels']} pixels")

    # --- extreme coordinates must still clip to contract (P1 of the 2026-08-29
    # review) --------------------------------------------------------------
    #
    # The far edge used to be computed in int16_t, so x + w - 1 overflowed and
    # **a rectangle that should have covered the whole screen vanished
    # instead.** Exhaustively, 28,441 (x, w) pairs disagreed with the int32
    # version.
    W = H = 64
    assert r["huge_rect_pixels"] == (W - 2) * (H - 2), (
        f"a rectangle from (2,2) of width 32767 sent {r['huge_rect_pixels']} pixels "
        f"(should be {(W - 2) * (H - 2)}; 0 means it vanished to overflow)")
    assert r["far_negative_pixels"] == 0, (
        f"a rectangle far to the top left reached the screen: {r['far_negative_pixels']} pixels")
    assert r["far_positive_pixels"] == 0, (
        f"a rectangle past the bottom right was drawn: {r['far_positive_pixels']} pixels")
    assert r["huge_clip_pixels"] == (W - 2) * (H - 2), (
        f"a huge clip rectangle sent {r['huge_clip_pixels']} pixels "
        f"(should be {(W - 2) * (H - 2)})")
