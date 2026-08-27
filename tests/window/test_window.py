"""回転・オフセットが MADCTL と CASET / RASET にどう出るか。

回転はコントローラの MADCTL でやる設計（docs/DECISIONS.ja.md D7）なので、
ソフト側で確かめられるのは「幅と高さの入れ替わり」「MADCTL の値」
「ウィンドウにオフセットが正しく乗るか」の 3 つ。

**MADCTL の値そのものが実機で正しいかは、ここでは分からない**（MANUAL_TEST M2）。
ここが守るのは「実装が下の表どおりに動いていること」。表が違っていたら実機で
分かるので、そのときはこの表も直す。
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent

MADCTL_MY, MADCTL_MX, MADCTL_MV = 0x80, 0x40, 0x20

# 135x240 パネル / GRAM 240x320 / 回転 0 のオフセット (52, 40)
#   cs2 = 240 - 135 - 52 = 53
#   rs2 = 320 - 240 - 40 = 40
EXPECTED = {
    0: dict(madctl=0x00, w=135, h=240, xs=52, ys=40),
    1: dict(madctl=MADCTL_MV | MADCTL_MX, w=240, h=135, xs=40, ys=53),
    2: dict(madctl=MADCTL_MX | MADCTL_MY, w=135, h=240, xs=53, ys=40),
    3: dict(madctl=MADCTL_MV | MADCTL_MY, w=240, h=135, xs=40, ys=52),
}


def test_window(dut):
    dut.expect("TEST start window", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    problems = []
    for rot, want in EXPECTED.items():
        for key, value in want.items():
            got = r[f"rot{rot}_{key}"]
            if got != value:
                problems.append(f"r={rot} {key}: {got} != {value}")
    assert not problems, "回転ごとの値が表と違う: " + "; ".join(problems)

    madctls = {r[f"rot{rot}_madctl"] for rot in range(4)}
    assert len(madctls) == 4, f"MADCTL が重複している: {sorted(madctls)}"

    for rot in range(4):
        xs, ys = r[f"zero{rot}_xs"], r[f"zero{rot}_ys"]
        assert (xs, ys) == (0, 0), f"オフセット無しのパネルで r={rot} が ({xs},{ys})"

    assert (r["clip_w"], r["clip_h"]) == (240, 135), "setRotation で width/height が入れ替わっていない"
