"""CellFont の連鎖と U+FFFD 退避。

生成されたフォントでは踏めない道を、手で組んだ小さな CellFont で通す。
仕様（LGFXFontToolJs docs/formats/cellfont.ja.md）の §7.1 / §7.2 / §8 / §15.2。
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent


def test_fontchain(dut):
    dut.expect("TEST start fontchain", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # 'A' は 3x5 を全部塗る。送り幅 4
    assert (r["a_ret"], r["a_lit"]) == (4, 15), f"'A' が {r['a_lit']} 画素 / 送り {r['a_ret']}"
    assert (r["a_top"], r["a_bottom"]) == (0, 4)

    # 収録外 -> U+FFFD へ退避。豆腐は上 1 行だけ（3 画素）
    assert r["nd_lit"] == 3, f"退避先が描かれていない（{r['nd_lit']} 画素）"
    assert r["nd_ret"] == 4, "退避しても送り幅は返る"
    assert (r["nd_top"], r["nd_bottom"]) == (0, 0)

    # 要求そのものが U+FFFD なら再検索しない（無限に潜らない）
    assert (r["ndself_lit"], r["ndself_ret"]) == (3, 4)

    # **本番。** 前段（fontAN）が U+FFFD を持っていても、後段の 'B' に到達すること。
    # デコーダの中で退避すると、ここが豆腐（3 画素・0 行目）になって落ちる。
    assert r["chain_lit"] == 9, (
        f"連鎖の後段の 'B' が出ていない（{r['chain_lit']} 画素）。"
        "前段の U+FFFD に潰されている疑い")
    # ベースラインは**先頭フォント**の ascent(5)。fontB は ascent 3 なので 2 行下に出る
    assert (r["chain_top"], r["chain_bottom"]) == (2, 4), (
        f"ベースラインが揃っていない（{r['chain_top']}..{r['chain_bottom']} 行）。"
        "デコーダが自分の ascent で換算している疑い")

    # どこにも無く、退避先も無い -> 何も描かず送り 0
    assert (r["miss_lit"], r["miss_ret"]) == (0, 0)

    # 疎索引: しっぽに first より小さいコードが居ても引けること（§7.1）。
    # グリフの並びは「頭ブロック -> しっぽ昇順」（§6）なので、
    # 頭の 'B' が index 0（全塗り 15 画素）、しっぽの 'A' が index 1（上 1 行 3 画素）。
    assert (r["lohead_lit"], r["lohead_ret"]) == (15, 4), "頭ブロックが引けていない"
    assert (r["lo_lit"], r["lo_ret"]) == (3, 4), "first より小さいコードが引けていない"

    # 行送りと ascent は連鎖の先頭のもの
    assert r["chain_line"] == 6
    assert r["chain_ascent"] == 5, "連鎖の ascent が先頭のものになっていない"
    assert r["b_ascent"] == 3
