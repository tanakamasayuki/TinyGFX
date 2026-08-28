"""実機と突き合わせるためのゴールデンを、ホストで作って凍結する。

**ゴールデンを実機の出力から作らない。** 実機がおかしくても「一致」してしまう。
ホスト（`TinyGFXBusCapture`）で描いたものを正とし、実機側
（`tests/hw/m5stack/`）はそれに合わせる。シーンの定義は
`tests/common_libs/tgfx_test/src/tgfx_scene.h` の 1 箇所だけ。
"""

from pathlib import Path

import pytest
import tgfx_check as tc

SKETCH = Path(__file__).parent
GOLDEN = SKETCH / "golden" / "scene.ppm"


def test_scene_matches_golden(dut):
    dut.expect("TEST start scene", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)
    # 実機の読み戻しは RGB666 で返る。飽和した色だけならバイト一致で比べられる
    assert r["unsaturated"] == 0, (
        f"飽和していない色が {r['unsaturated']} 画素ある。"
        "実機の読み戻しと往復で 1 LSB ずれるので、シーンには使わないこと")

    produced = (SKETCH / "output" / "scene.ppm").read_bytes()
    if not GOLDEN.exists():
        GOLDEN.parent.mkdir(parents=True, exist_ok=True)
        GOLDEN.write_bytes(produced)
        pytest.fail(
            f"ゴールデンが無かったので作った: {GOLDEN}\n"
            "**目で見てからコミットすること。** 一度凍結したら、"
            "変わったときはシーンを変えたのか壊したのかを判断する")

    assert produced == GOLDEN.read_bytes(), (
        "シーンの出力がゴールデンと違う。シーンを変えたなら golden/scene.ppm を"
        "更新し、実機側（tests/hw/m5stack/）も測り直すこと")
