"""u8g2 形式デコーダ（測定用の試作）が正しく描けているか。

参照は LGFXFontToolJs が同じフォント・同じ文字列を描いた絵
（`tools/gen_u8g2_ref.mjs` が生成）。原点の流儀が違うので、
**墨の外接矩形で切り出してから**比べる。
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent


def art(name):
    """描いた結果を、墨の外接矩形で切り出したテキストアートにする。"""
    im = tc.image(SKETCH, name)
    on = tc.lit(im)
    assert on, f"{name}: 1 画素も描かれていない"
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
    assert r["line_height"] == 9, f"行送りが {r['line_height']}"
    assert r["ascii_width"] == 64, f"描いた幅が {r['ascii_width']}"
    assert r["ascii_measured"] == r["ascii_width"], "textWidth と drawString の戻り値が違う"
    assert r["cjk_width"] == 40, f"CJK の幅が {r['cjk_width']}"
    assert r["missing_adv"] == 0, "収録外の文字が送り幅を返している"
    assert r["missing_pixels"] == 0, "収録外の文字が画素を描いている"

    for name in ["ascii", "cjk"]:
        got, want = art(name), reference(name)
        assert got == want, f"{name} が参照と違う:\n{diff(got, want)}"
