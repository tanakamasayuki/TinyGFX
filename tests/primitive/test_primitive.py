"""The primitives. Weighted towards the last pixel at each edge and the
degenerate cases."""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent

MUST_BE_ZERO = ["w0", "h0", "wneg", "hline0", "rect0", "offscreen", "rightout",
                "pixelneg", "pixelover", "rneg"]


def test_primitive(dut):
    dut.expect("TEST start primitive", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)
    img = lambda n: tc.image(SKETCH, n)
    lit = tc.lit

    # --- degenerate cases: not one pixel may be sent ------------------------
    nonzero = {k: r[f"degen_{k}"] for k in MUST_BE_ZERO if r[f"degen_{k}"]}
    assert not nonzero, f"calls that should draw nothing sent pixels: {nonzero}"
    assert r["degen_r0"] >= 1, "a circle of radius 0 drew nothing at all"
    assert r["degen_dot_line"] == 1, f"a single-point line drew {r['degen_dot_line']} pixels"
    assert r["degen_clipped_rect"] == 6 * 6, (
        f"a 10x10 hanging off the screen sent {r['degen_clipped_rect']} pixels (should be 6x6)")

    # --- straight lines -----------------------------------------------------
    assert lit(img("hline")) == {(x, 30) for x in range(4, 24)}
    assert lit(img("vline")) == {(30, y) for y in range(4, 24)}
    assert lit(img("onepixel")) == {(10, 10)}

    line = lit(img("line"))
    assert (2, 2) in line and (40, 30) in line, "the line is missing an end point"

    # --- rectangle: outline only, hollow inside -----------------------------
    rect = lit(img("drawrect"))
    assert (8, 8) in rect and (23, 19) in rect, "a corner is missing"
    assert (16, 13) not in rect, "an outline filled its interior"
    assert len(rect) == 2 * 16 + 2 * (12 - 2), f"wrong number of outline pixels: {len(rect)}"

    # --- circles ------------------------------------------------------------
    circle = lit(img("circle"))
    for p in [(32, 12), (32, 52), (12, 32), (52, 32)]:
        assert p in circle, f"the circle is missing its extreme at {p}"
    assert (32, 32) not in circle, "drawCircle painted its centre"

    filled = lit(img("fillcircle"))
    assert (32, 32) in filled, "fillCircle left its centre empty"
    for p in [(32, 12), (32, 52), (12, 32), (52, 32)]:
        assert p in filled, f"fillCircle is missing its extreme at {p}"
    assert (12, 12) not in filled, "a corner outside the circle was painted"

    # --- rounded rectangles -------------------------------------------------
    rr = lit(img("roundrect"))
    assert (6, 6) not in rr, "the corner is square on a rounded rectangle"
    assert (26, 6) in rr, "the top edge is missing"
    frr = lit(img("fillroundrect"))
    assert (6, 6) not in frr, "the filled rounded rectangle kept its square corner"
    assert (26, 20) in frr, "the filled rounded rectangle is hollow"

    # --- triangles ----------------------------------------------------------
    tri = lit(img("triangle"))
    for p in [(4, 60), (32, 4), (60, 60)]:
        assert p in tri, f"the triangle is missing its vertex at {p}"
    assert (32, 40) not in tri, "drawTriangle filled its interior"

    ftri = lit(img("filltriangle"))
    assert (32, 40) in ftri, "fillTriangle is hollow"
    assert (4, 4) not in ftri, "something outside the triangle was painted"
    assert len(ftri) > len(tri), "the fill is smaller than the outline"

    # --- hanging off the screen ---------------------------------------------
    assert lit(img("clipped_rect")) == {(x, y) for x in range(6) for y in range(6)}
