"""クリップの不変条件。

クリップ内はクリップ無しと 1 画素も違わず、外は 1 画素も触られないこと。
期待画像を持たずに済むので、絵が変わってもテストは壊れない。
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
BLACK = (0, 0, 0)
CX, CY, CW, CH = 10, 12, 30, 26


def test_clip(dut):
    dut.expect("TEST start clip", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)
    free = tc.image(SKETCH, "free").load()
    clipped = tc.image(SKETCH, "clipped").load()

    inside_mismatch, outside_dirty = [], []
    for y in range(64):
        for x in range(64):
            inside = CX <= x < CX + CW and CY <= y < CY + CH
            if inside:
                if free[x, y] != clipped[x, y]:
                    inside_mismatch.append((x, y))
            elif clipped[x, y] != BLACK:
                outside_dirty.append((x, y))

    assert not outside_dirty, (
        f"クリップの外が {len(outside_dirty)} 画素塗られている: {outside_dirty[:8]}")
    assert not inside_mismatch, (
        f"クリップ内がクリップ無しと違う: {len(inside_mismatch)} 画素 {inside_mismatch[:8]}")

    # 中身が空だと上の 2 つは自明に通ってしまうので、実際に描けていることを確かめる
    drawn = sum(1 for y in range(CY, CY + CH) for x in range(CX, CX + CW)
                if clipped[x, y] != BLACK)
    assert drawn > CW * CH // 4, f"クリップ内がほとんど空: {drawn} 画素"

    assert r["clipped_pixels"] < r["free_pixels"], (
        "クリップしても転送量が減っていない"
        f"（{r['clipped_pixels']} vs {r['free_pixels']}）")
    assert r["oversize_clip_pixels"] == 64 * 64, (
        f"画面より大きいクリップで {r['oversize_clip_pixels']} 画素（4096 のはず）")
    assert r["empty_clip_pixels"] == 0, (
        f"空のクリップで {r['empty_clip_pixels']} 画素送っている")
