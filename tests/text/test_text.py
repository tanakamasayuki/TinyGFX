"""文字描画。

フォントは **本番の生成器（LGFXFontToolJs CLI）が出したもの**を tgfx_font に
置いてある。`lgfxJapanGothic_8` の数字 10 字で、墨面 4x6・送り幅 4・行送り 9。

同じ 10 字を 3 通りに符号化したものを並べてあり、**どれで描いても絵が一致すること**
を最後に見る（どの符号化を選ぶかは生成器の裁量で、スケッチから見えてはいけない）。
"""

from pathlib import Path

from PIL import ImageChops

import tgfx_check as tc

SKETCH = Path(__file__).parent
BLACK, BLUE = tc.BLACK, tc.BLUE
ADV, LINE = 4, 9


def test_text(dut):
    dut.expect("TEST start text", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    assert r["font_height"] == LINE, f"fontHeight={r['font_height']}"
    assert r["text_width"] == 5 * ADV, f"textWidth('12345')={r['text_width']}"
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

    # --- 中央揃え・右揃え ----------------------------------------------------
    #
    # **不変条件で見る。** 揃え版と「算出位置に置いた drawString」が 1 画素も
    # 違わないこと。絶対座標で期待値を書くとグリフの左余白に依存してしまう。
    #
    # LovyanGFX はこれを setTextDatum でも提供しているが、TinyGFX は明示関数
    # だけにした。datum は drawString が毎回参照する状態になるので、中央揃えを
    # 使わない人まで 204 B 払う（CH32V003 で実測。明示関数は呼ばなければ 0）。
    w = r["plain_width"]
    assert w > 0, "textWidth が 0"
    assert r["center_matches"] == 1, "drawCenterString が drawString(cx - w/2) と違う"
    assert r["right_matches"] == 1, "drawRightString が drawString(rx - w) と違う"
    assert r["center_moved"] == 1, "中央揃えと右揃えが同じ位置に描かれている"
    assert r["center_ret"] == w, f"drawCenterString の戻り値が {r['center_ret']}（{w} のはず）"
    assert r["right_ret"] == w, f"drawRightString の戻り値が {r['right_ret']}（{w} のはず）"
