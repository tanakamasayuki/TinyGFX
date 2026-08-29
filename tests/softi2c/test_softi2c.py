"""**ビット叩きの I2C が Wire と同じバイト列を出すか。**

実機が無くても線の上の波形は見られる。ホストコアのピンフックで **I2C の
バスそのものを模す**:

- オープンドレイン: `OUTPUT`+`LOW` で引き下げ、`INPUT` で手放す
- **読み出しフックがプルアップの役** —— 手放されたピンは `HIGH` に見える
- SCL の立ち上がりで SDA を読み、START / STOP を検出してバイトに戻す

戻したバイト列を SSD1306 の模型に流し、**Wire 経由で同じ絵を描いた結果と
突き合わせる。** 1 バイトでも違えば実装が違う。

`TinyGFXBusSoftI2C` があるのは大きさのためではなくピンのため。ハード I2C は
固定ピンにしか出ないので、CH32V003 のようにピンの少ない部品ではそこを別の
用途に使いたいことがある。**AVR では副産物として 1,444 B 小さくもなる**
（AVR の Wire がバッファと割り込み駆動の状態機械を持つため）。
"""

from pathlib import Path

import pytest

import tgfx_check as tc

SKETCH = Path(__file__).parent
W, H = 128, 64


def test_softi2c(dut):
    dut.expect("TEST start softi2c|TEST skip softi2c", timeout=20)
    if not (SKETCH / "output").exists():
        pytest.skip("ピンフックを持たないホストコア")
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- 波形として成立していること -------------------------------------------
    assert r["soft_starts"] > 0, "START が 1 回も出ていない"
    assert r["soft_stops"] == r["soft_starts"], (
        f"START {r['soft_starts']} 回に対して STOP {r['soft_stops']} 回")
    assert r["soft_bytes_seen"] > 0, "1 バイトも読めていない"

    # --- 転送量が Wire と一致すること ------------------------------------------
    assert r["soft_data_bytes"] == r["wire_data_bytes"], (
        f"画素バイト数が違う: ソフト {r['soft_data_bytes']} / "
        f"Wire {r['wire_data_bytes']}")

    # --- **本命。** 同じ絵になること -------------------------------------------
    assert r["soft_vs_wire_diff"] == 0, (
        f"ソフト I2C と Wire で {r['soft_vs_wire_diff']} バイト違う")

    # 絵が単色で「一致」しても意味がない
    assert 0 < r["lit"] < W * H, f"絵が単色（点灯 {r['lit']} / {W * H}）"
