"""pushImage の配置と切り取り、transparent 版。"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
RED, GREEN, BLUE = tc.RED, tc.GREEN, tc.BLUE
WHITE, BLACK = tc.WHITE, tc.BLACK


def test_image(dut):
    dut.expect("TEST start image", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- 転送量 -------------------------------------------------------------
    assert r["plain_pixels"] == 16, f"4x4 が {r['plain_pixels']} 画素"
    assert r["topleft_pixels"] == 4, f"左上へ半分はみ出した 4x4 が {r['topleft_pixels']} 画素"
    assert r["bottomright_pixels"] == 4, f"右下へ半分はみ出した 4x4 が {r['bottomright_pixels']} 画素"
    assert r["clipped_pixels"] == 4, f"クリップされた 4x4 が {r['clipped_pixels']} 画素"
    assert r["transparent_pixels"] == 12, (
        f"赤 4 画素を抜いた 4x4 が {r['transparent_pixels']} 画素（12 のはず）")
    assert r["offscreen_pixels"] == 0, "画面外の pushImage が画素を送っている"
    assert r["zero_pixels"] == 0, "幅 0 の pushImage が画素を送っている"

    # --- 配置 ---------------------------------------------------------------
    p = tc.image(SKETCH, "plain").load()
    assert p[2, 2] == RED and p[5, 2] == GREEN, "上段の並びが違う"
    assert p[2, 5] == BLUE and p[5, 5] == WHITE, "下段の並びが違う"
    assert p[1, 2] == BLACK and p[6, 2] == BLACK, "左右へはみ出している"

    # 左上へはみ出した場合、見えるのは画像の右下 2x2
    p = tc.image(SKETCH, "topleft").load()
    assert p[0, 0] == WHITE, f"左上の切り取りがずれている: {p[0, 0]}"
    assert p[2, 0] == BLACK, "切り取り後に余計な画素が出ている"

    # 右下へはみ出した場合、見えるのは画像の左上 2x2
    p = tc.image(SKETCH, "bottomright").load()
    assert p[14, 14] == RED, f"右下の切り取りがずれている: {p[14, 14]}"

    # クリップ (4,4,4,4) と画像 (2,2,4,4) の重なりは (4,4)-(5,5) の 2x2
    p = tc.image(SKETCH, "clipped").load()
    assert p[4, 4] == WHITE, f"クリップ内が違う: {p[4, 4]}"
    assert p[3, 4] == BLACK and p[6, 4] == BLACK, "クリップの外へ出ている"

    # 透過: 赤の 4 画素だけ背景のまま
    p = tc.image(SKETCH, "transparent").load()
    assert p[2, 2] == BLACK and p[3, 3] == BLACK, "透過色が描かれてしまっている"
    assert p[4, 2] == GREEN and p[2, 4] == BLUE, "透過以外が欠けている"
