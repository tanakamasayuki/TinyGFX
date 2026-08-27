"""プリミティブの正しさ。端の 1 画素と縮退ケースを重点的に見る。"""

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

    # --- 縮退ケース: 1 画素も送ってはいけない ---------------------------------
    nonzero = {k: r[f"degen_{k}"] for k in MUST_BE_ZERO if r[f"degen_{k}"]}
    assert not nonzero, f"何も描かないはずの呼び出しが画素を送っている: {nonzero}"
    assert r["degen_r0"] >= 1, "半径 0 の円が 1 画素も出ていない"
    assert r["degen_dot_line"] == 1, f"1 点の線が {r['degen_dot_line']} 画素"
    assert r["degen_clipped_rect"] == 6 * 6, (
        f"画面外にはみ出した 10x10 が {r['degen_clipped_rect']} 画素（6x6 のはず）")

    # --- 直線 ---------------------------------------------------------------
    assert lit(img("hline")) == {(x, 30) for x in range(4, 24)}
    assert lit(img("vline")) == {(30, y) for y in range(4, 24)}
    assert lit(img("onepixel")) == {(10, 10)}

    line = lit(img("line"))
    assert (2, 2) in line and (40, 30) in line, "線の端点が無い"

    # --- 矩形: 枠だけで中は空 -----------------------------------------------
    rect = lit(img("drawrect"))
    assert (8, 8) in rect and (23, 19) in rect, "角が無い"
    assert (16, 13) not in rect, "枠なのに中が塗られている"
    assert len(rect) == 2 * 16 + 2 * (12 - 2), f"枠の画素数が合わない: {len(rect)}"

    # --- 円 -----------------------------------------------------------------
    circle = lit(img("circle"))
    for p in [(32, 12), (32, 52), (12, 32), (52, 32)]:
        assert p in circle, f"円の端 {p} が無い"
    assert (32, 32) not in circle, "drawCircle なのに中心が塗られている"

    filled = lit(img("fillcircle"))
    assert (32, 32) in filled, "fillCircle の中心が空"
    for p in [(32, 12), (32, 52), (12, 32), (52, 32)]:
        assert p in filled, f"fillCircle の端 {p} が無い"
    assert (12, 12) not in filled, "円の外（角）が塗られている"

    # --- 角丸 ---------------------------------------------------------------
    rr = lit(img("roundrect"))
    assert (6, 6) not in rr, "角丸なのに角が尖っている"
    assert (26, 6) in rr, "上辺が無い"
    frr = lit(img("fillroundrect"))
    assert (6, 6) not in frr, "塗り角丸の角が落ちていない"
    assert (26, 20) in frr, "塗り角丸の中が空"

    # --- 三角形 --------------------------------------------------------------
    tri = lit(img("triangle"))
    for p in [(4, 60), (32, 4), (60, 60)]:
        assert p in tri, f"三角形の頂点 {p} が無い"
    assert (32, 40) not in tri, "drawTriangle なのに中が塗られている"

    ftri = lit(img("filltriangle"))
    assert (32, 40) in ftri, "fillTriangle の中が空"
    assert (4, 4) not in ftri, "三角形の外が塗られている"
    assert len(ftri) > len(tri), "塗りが枠より小さい"

    # --- 画面外へのはみ出し ---------------------------------------------------
    assert lit(img("clipped_rect")) == {(x, y) for x in range(6) for y in range(6)}
