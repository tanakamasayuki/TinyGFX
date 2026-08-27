"""転送量の過不足。ウィンドウ計算がずれると多すぎるか足りなくなる。"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
W, H = 40, 30

EXPECTED = {
    "fillscreen": W * H,
    "r1x1": 1,
    "rowfull": W,
    "colfull": H,
    "r7x11": 7 * 11,
    "hline13": 13,
    "vline9": 9,
    "corner_clip": 3 * 3,
    "topleft_clip": 5 * 5,
    "clipped_screen": 8 * 8,
    "fillscreen_rot1": W * H,
}


def test_fill(dut):
    dut.expect("TEST start fill", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)
    wrong = {k: (r[f"n_{k}"], v) for k, v in EXPECTED.items() if r[f"n_{k}"] != v}
    assert not wrong, "転送画素数が期待と違う（実測, 期待）: " + repr(wrong)
