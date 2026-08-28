"""文字描画。

フォントは 5x7（送り幅 6 / 行送り 8 / 収録 0x20-0x3F）。
ライブラリはフォントを同梱しないので、これはテスト側の tgfx_font から来ている。
"""

from pathlib import Path

from PIL import ImageChops

import tgfx_check as tc

SKETCH = Path(__file__).parent
BLACK, BLUE = tc.BLACK, tc.BLUE
ADV, LINE = 6, 8


def test_text(dut):
    dut.expect("TEST start text", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    assert r["font_height"] == LINE, f"fontHeight={r['font_height']}"
    assert r["text_width"] == 5 * ADV, f"textWidth('12:34')={r['text_width']}"
    assert r["draw_width"] == r["text_width"], (
        f"drawString の戻り値 {r['draw_width']} が textWidth {r['text_width']} と違う")
    assert r["width_x2"] == 2 * 5 * ADV, f"倍角の幅が {r['width_x2']}"
    assert r["height_x2"] == 2 * LINE, f"倍角の高さが {r['height_x2']}"

    # 収録外の文字
    assert r["oor_advance"] == 0, "収録されていない文字が送り幅を返している"
    assert r["oor_pixels"] == 0, "収録されていない文字が画素を送っている"

    # --- 透過描画: グリフの画素だけ ------------------------------------------
    plain = tc.image(SKETCH, "plain")
    on = tc.lit(plain)
    assert on, "文字が 1 画素も描かれていない"
    xs = [x for x, _ in on]
    ys = [y for _, y in on]
    assert min(xs) >= 2 and max(xs) < 2 + 5 * ADV, f"横のはみ出し: {min(xs)}..{max(xs)}"
    assert min(ys) >= 3 and max(ys) < 3 + LINE, f"縦のはみ出し: {min(ys)}..{max(ys)}"

    # 倍角は素の 2 倍の面積あたりに広がる
    dbl = tc.lit(tc.image(SKETCH, "double"))
    assert max(x for x, _ in dbl) >= 3 * ADV, "倍角が広がっていない"

    # --- 背景色つき: セルが埋まる --------------------------------------------
    opaque = tc.image(SKETCH, "opaque").load()
    holes = [(x, y)
             for y in range(3, 3 + LINE)
             for x in range(2, 2 + 5 * ADV)
             if opaque[x, y] == BLACK]
    assert not holes, f"背景指定なのにセルに穴がある: {len(holes)} 画素 {holes[:8]}"
    assert opaque[1, 3] == BLACK, "セルの外まで背景が塗られている"
    assert opaque[2, 3 + LINE] == BLACK, "セルの下まで背景が塗られている"

    # --- 透過は背景を残す ----------------------------------------------------
    tr = tc.image(SKETCH, "transparent").load()
    assert tr[2, 3] == BLACK or tr[2 + 5, 3] == BLACK, (
        "透過なのにグリフの隙間が塗られている")

    # --- CellFont の変種は同じ絵になること ------------------------------------
    # 索引（連続 / 疎）とグリフ表（無し / 有り）は生成時の選択でしかない。
    # どれを選んでも描画結果は 1 画素も変わらないこと。
    ref = tc.image(SKETCH, "var_fixed")
    assert tc.lit(ref), "基準の変種が 1 画素も描けていない"
    for name in ["var_records", "var_sparse"]:
        img = tc.image(SKETCH, name)
        box = ImageChops.difference(ref, img).getbbox()
        assert box is None, f"{name} が固定ピッチ版と違う: bbox={box}"
