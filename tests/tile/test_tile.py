"""帯レンダリングの不変条件。

**帯の行数をいくつに変えても、直接描いた結果と 1 画素も違わないこと。**
期待画像を持たずに済むので、シーンを変えてもテストは壊れない
（LGFXVirtualCanvas の parity と同じ考え方）。
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

    # 帯の行数はバッファ画素数 ÷ 幅
    for rows in ROWS:
        assert r[f"rows{rows}_tilerows"] == rows, (
            f"バッファ {W}x{rows} で tileRows()={r[f'rows{rows}_tilerows']}")
        assert r[f"rows{rows}_ok"] == 1, f"rows={rows} の render が false"

    # --- 本題: どの行数でも直接描画と一致すること --------------------------
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
        "帯の行数で絵が変わっている（output/diff_tile*.ppm を見ること）: "
        + "; ".join(failures))

    # 中身が空だと上の比較が自明に通るので、実際に描けていることを確かめる
    assert len(tc.colors(direct)) >= 4, f"基準画像の色数が少なすぎる: {tc.colors(direct)}"

    # --- ラムダで渡しても同じ絵 --------------------------------------------
    #
    # 関数ポインタ + void* と、任意の callable を取るテンプレートは通る道が
    # 違う（後者はトランポリンを 1 枚挟む）。**同じ絵、同じ転送画素数**で
    # なければならない。実測ではサイズも同じか小さい（DECISIONS.ja.md D28）。
    assert r["lambda_diff"] == 0, (
        f"ラムダで渡すと {r['lambda_diff']} 画素違う")
    assert r["lambda_pixels"] == r["fnptr_pixels"], (
        f"転送画素数が違う: ラムダ {r['lambda_pixels']} / "
        f"関数ポインタ {r['fnptr_pixels']}")

    # 捕捉した変数が実際に更新されていること（トランポリンが同じ実体を呼んでいる）
    from math import ceil
    expected_bands = ceil(H / r["lambda_tilerows"])
    assert r["lambda_bands"] == expected_bands, (
        f"ラムダが {r['lambda_bands']} 回呼ばれた。帯は {expected_bands} 本")

    # --- バッファが足りない場合 --------------------------------------------
    assert r["toosmall_rows"] == 0, "1 行ぶんも無いのに帯が作れている"
    assert r["toosmall_ok"] == 0, "1 行ぶんも無いのに render が成功している"

    # --- 自動クリアを切ると絵が変わる（切れていることの確認） ----------------
    noac = tc.image(SKETCH, "noautoclear")
    assert ImageChops.difference(direct, noac).getbbox() is not None, (
        "setAutoClear(false) でも自動クリア時と同じ絵になっている")
