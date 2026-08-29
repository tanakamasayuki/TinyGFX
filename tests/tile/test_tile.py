"""The tiled-rendering invariant.

**Whatever the band height, not one pixel differs from drawing directly.**
Stating it that way needs no expected image, so changing the scene does not
break the test (the same idea as LGFXVirtualCanvas's parity).
"""

from pathlib import Path

from PIL import ImageChops

import tgfx_check as tc

SKETCH = Path(__file__).parent
ROWS = [1, 2, 3, 5, 7, 8]
W = H = 48


def test_tile(dut):
    dut.expect("TEST start tile", timeout=20)
    dut.expect("TEST done", timeout=120)

    r = tc.report(SKETCH)
    direct = tc.image(SKETCH, "direct")

    # Band rows = buffer pixels / width
    for rows in ROWS:
        assert r[f"rows{rows}_tilerows"] == rows, (
            f"a {W}x{rows} buffer gave tileRows()={r[f'rows{rows}_tilerows']}")
        assert r[f"rows{rows}_ok"] == 1, f"render returned false at rows={rows}"

    # --- the point: every band height matches direct drawing ----------------
    failures = []
    for rows in ROWS:
        img = tc.image(SKETCH, f"tile{rows}")
        assert img.size == direct.size
        diff = ImageChops.difference(direct, img)
        box = diff.getbbox()
        if box is not None:
            diff.point(lambda v: 255 if v else 0).save(
                SKETCH / "output" / f"diff_tile{rows}.ppm")
            failures.append(f"rows={rows} bbox={box}")
    assert not failures, (
        "the band height changed the picture (see output/diff_tile*.ppm): "
        + "; ".join(failures))

    # The comparison above passes trivially on an empty picture, so check
    # something was actually drawn.
    assert len(tc.colors(direct)) >= 4, f"too few colours in the baseline: {tc.colors(direct)}"

    # --- a lambda draws the same picture ------------------------------------
    #
    # The function pointer + void* form and the template that takes any
    # callable go through different code (the latter via a trampoline).
    # **Same picture, same pixel count.** Measured, the size is the same or
    # smaller too (DECISIONS.ja.md D28).
    assert r["lambda_diff"] == 0, (
        f"passing a lambda changed {r['lambda_diff']} pixels")
    assert r["lambda_pixels"] == r["fnptr_pixels"], (
        f"different pixel counts: lambda {r['lambda_pixels']} / "
        f"function pointer {r['fnptr_pixels']}")

    # The captured variable really is updated (the trampoline calls the same
    # object, not a copy).
    from math import ceil
    expected_bands = ceil(H / r["lambda_tilerows"])
    assert r["lambda_bands"] == expected_bands, (
        f"the lambda ran {r['lambda_bands']} times; there are {expected_bands} bands")

    # --- a buffer too small to hold a row -----------------------------------
    assert r["toosmall_rows"] == 0, "a band was formed from less than one row"
    assert r["toosmall_ok"] == 0, "render succeeded with less than one row"

    # --- turning auto-clear off changes the picture (proof it is off) -------
    noac = tc.image(SKETCH, "noautoclear")
    assert ImageChops.difference(direct, noac).getbbox() is not None, (
        "setAutoClear(false) still produced the auto-cleared picture")
