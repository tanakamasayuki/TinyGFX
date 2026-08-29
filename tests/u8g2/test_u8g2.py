"""Does the u8g2-format decoder draw what it should?

The reference is what LGFXFontToolJs draws for the same font and the same
string (produced by `tools/gen_u8g2_ref.mjs`). The two disagree about where the
origin is, so the comparison is made **after cropping to the ink**.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent


def art(name):
    """The drawing as text art, cropped to the bounding box of the ink."""
    im = tc.image(SKETCH, name)
    on = tc.lit(im)
    assert on, f"{name}: not one pixel was drawn"
    x0 = min(x for x, _ in on)
    x1 = max(x for x, _ in on)
    y0 = min(y for _, y in on)
    y1 = max(y for _, y in on)
    return "\n".join(
        "".join("#" if (x, y) in on else "." for x in range(x0, x1 + 1))
        for y in range(y0, y1 + 1)
    )


def reference(name):
    return (SKETCH / f"u8g2_{name}.ref.txt").read_text(encoding="utf-8").rstrip("\n")


def diff(got, want):
    g, w = got.splitlines(), want.splitlines()
    out = [f"  size: got {len(g[0]) if g else 0}x{len(g)} / want {len(w[0]) if w else 0}x{len(w)}"]
    for i in range(max(len(g), len(w))):
        gl = g[i] if i < len(g) else ""
        wl = w[i] if i < len(w) else ""
        out.append(f"  {i:2d} got  {gl}")
        if gl != wl:
            out.append(f"     want {wl}")
    return "\n".join(out)


def test_u8g2(dut):
    dut.expect("TEST start u8g2", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)
    assert r["line_height"] == 9, f"line height is {r['line_height']}"
    assert r["ascii_width"] == 64, f"drawn width is {r['ascii_width']}"
    assert r["ascii_measured"] == r["ascii_width"], "textWidth and drawString disagree"
    assert r["cjk_width"] == 40, f"CJK width is {r['cjk_width']}"
    assert r["missing_adv"] == 0, "an uncovered code returned an advance"
    assert r["missing_pixels"] == 0, "an uncovered code drew pixels"

    for name in ["ascii", "cjk"]:
        got, want = art(name), reference(name)
        assert got == want, f"{name} differs from the reference:\n{diff(got, want)}"
