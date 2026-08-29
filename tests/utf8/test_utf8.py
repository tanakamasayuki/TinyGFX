"""UTF-8 の復号。

スケッチの文字列は、誰が選んだわけでもなく UTF-8 になっている（Arduino IDE が
そう保存する）。なので検査したいのは「CJK が出るか」より前に、**普通の文字列に
度記号が 1 つ混ざっているとき、1 文字として出るか**のほうである。

`TinyGFX::nextCode` は純関数なので、画素ではなく**コードポイントと消費バイト数**を
直接見る。正しいコードポイントを返しても消費バイト数が違えば、その後ろが全部
ずれるので、2 つ目の数字も同じだけ重要。

フォントは `tgfx_utf8`（1 バイト・2 バイト・3 バイトを 1 つのフォントに含む）。
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent

NOTDEF = 0xFFFD


def test_utf8(dut):
    dut.expect("TEST start utf8", timeout=20)
    dut.expect("TEST done", timeout=60)
    r = tc.report(SKETCH)

    # --- 復号器そのもの ---------------------------------------------------
    # (コードポイント, 消費バイト数)
    expected = {
        "ascii":    (0x41, 1),
        "two":      (0x00B0, 2),      # U+00B0 DEGREE SIGN
        "three":    (0x2103, 3),      # U+2103 DEGREE CELSIUS
        "four":     (NOTDEF, 4),      # U+1F600。U+FFFF を超えるので notdef、ただし 4 バイト食う
        "stray":    (NOTDEF, 1),      # 先導のない継続バイト
        "ff":       (NOTDEF, 1),      # UTF-8 には現れないバイト
        "cut_end":  (NOTDEF, 1),      # 先導のあと終端。**終端を飛び越えないこと**
        "cut_mid":  (NOTDEF, 2),      # 先導 + 継続 1 個で途切れる
        "overlong": (0x0000, 3),      # 冗長符号化は復号する（拒否しない）。仕様どおり
    }
    for name, (cp, used) in expected.items():
        assert r[f"{name}_cp"] == cp, (
            f"{name}: コードポイントが {r[f'{name}_cp']:#06x}、期待 {cp:#06x}")
        assert r[f"{name}_used"] == used, (
            f"{name}: {r[f'{name}_used']} バイト消費、期待 {used}")

    # 途切れた列のあとは、壊したバイトから読み直せること
    assert r["recover_bad"] == NOTDEF
    assert r["recover_next"] == 0x41, "途切れた列の次の 'A' が読めていない"
    assert r["recover_used"] == 2, "2 バイトの入力で消費が合わない"

    # --- 文字列 -----------------------------------------------------------
    # "0°℃" は 3 文字 6 バイト。3 文字ぶんの送り幅の和になること。
    # このフォントには U+FFFD が無いので、バイト単位で読むと '0' の 4 だけになる。
    total = r["w_c0"] + r["w_deg"] + r["w_cel"]
    assert r["w_mixed"] == total, (
        f'textWidth("0°℃") が {r["w_mixed"]}、1 文字ずつの和は {total}')
    assert r["w_mixed"] > r["w_c0"], "多バイト文字が 1 文字も数えられていない"
    assert r["draw_mixed"] == r["w_mixed"], (
        f"drawString の戻り値 {r['draw_mixed']} が textWidth {r['w_mixed']} と違う")

    # 収録外の 4 バイト文字は何も描かないが、4 バイト食うこと。
    # 食い損ねると後ろの '1' の位置がずれる。
    assert r["astral_end"] == r["plain_end"], (
        f"4 バイト文字のあとの位置が {r['astral_end']}、"
        f"その文字が無い場合は {r['plain_end']}")

    # --- Print ------------------------------------------------------------
    # Print は 1 バイトずつ来るので途中の状態を持つ。同じ絵になること。
    assert r["print_last_col"] == r["mixed_last_col"], (
        f"print の右端が {r['print_last_col']}、drawString は {r['mixed_last_col']}")
    assert tc.image(SKETCH, "print") == tc.image(SKETCH, "mixed"), (
        "print と drawString で絵が違う")

    assert r["print_astral_end"] == r["plain_end"], (
        f"print で 4 バイト文字のあとの位置が {r['print_astral_end']}、"
        f"その文字が無い場合は {r['plain_end']}")

    # 列の途中で改行が来ても改行であること
    assert r["print_cut_nl_x"] > 0, "改行後に '0' が描かれていない"
    assert r["print_cut_nl_y"] == r["line_height"], (
        f"改行で y が {r['print_cut_nl_y']}、行送りは {r['line_height']}")
