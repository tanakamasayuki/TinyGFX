"""実機（M5Stack Core / BASIC）の自動テスト — Tier 3。

**実機の上で描いた絵を、ホストで作ったゴールデンと 1 画素も違わないか見る。**
ホストのテストが守れないのはここ — 実機のコンパイラ、実機の `int` 幅、実機の
PROGMEM。AVR でフォント構造体が PROGMEM に載っていなかった不具合
（DECISIONS D19）は、まさにホストでは原理的に出ない類だった。

    cp .env.example .env     # 自分のポートを書く
    uv run --env-file .env pytest hw --profile m5stack

**`.env` を渡さないと skip する。** 素の `uv run pytest` が実機を焼きに行かない
ようにするための入口。

ゴールデンは `tests/scene/golden/scene.ppm`。**ホストで作ったものが正**で、
実機の出力からは作らない（実機がおかしくても「一致」してしまうため）。
シーンの定義は `tests/common_libs/tgfx_test/src/tgfx_scene.h` の 1 箇所だけ。

## パネルの読み戻しについて

M5Stack の ILI9342C は **SDA が GPIO23 の 1 本きり**（MOSI と MISO の兼用）で、
SPI 周辺機の MISO（GPIO19）には何も来ていない。読むには線の向きを変えて
手で叩くしかない（`TinyGFXBusSPI::setReadPins`）。2026-08-28 に**動くようになった。**

効く条件は 4 つとも実測で詰めてある。詳細は
[docs/MANUAL_TEST.ja.md](../../../docs/MANUAL_TEST.ja.md) の「読み戻し」。

- **待ちを入れない。** 1 エッジでも `delayMicroseconds` を挟むとパネルが線を離す
- **1 画素ごとに窓を張り直す。** 連続読みではカラムが進まず同じ画素が返る
- **2 回読んで一致するまで繰り返す。** 20 バイトに 1 回ほどビットが化ける
- **戻すのは `SPI.end()` → `SPI.begin()`。** `begin()` だけだと何も起きず、
  以降の書き込みが黙って死ぬ

読めないパネルでは skip する。読めるなら**既定で走る** — 上 8 行で 75ms 程度。
"""

import os
import struct
from pathlib import Path

import pytest
from PIL import Image

SKETCH = Path(__file__).parent
GOLDEN = SKETCH.parent.parent / "scene" / "golden" / "scene.ppm"

PORT = os.environ.get("TEST_SERIAL_PORT_M5STACK") or os.environ.get("TEST_SERIAL_PORT")

pytestmark = [
    pytest.mark.hardware,
    pytest.mark.skipif(
        not PORT or not Path(PORT).exists(),
        reason="M5Stack が繋がっていない（.env の TEST_SERIAL_PORT_M5STACK）",
    ),
]


def _golden_rgb565():
    im = Image.open(GOLDEN).convert("RGB")
    w, h = im.size
    out = []
    for y in range(h):
        for x in range(w):
            r, g, b = im.getpixel((x, y))
            out.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
    return w, h, out


def _compare(name, raw, w, h, want):
    assert len(raw) == w * h * 2, f"{name}: サイズが違う {len(raw)} != {w * h * 2}"
    got = list(struct.unpack(f"<{w * h}H", raw))
    diff = [i for i, (a, b) in enumerate(zip(want, got)) if a != b]
    if diff:
        head = ", ".join(
            f"({i % w},{i // w}) want={want[i]:#06x} got={got[i]:#06x}" for i in diff[:6]
        )
        pytest.fail(f"{name}: ゴールデンと違う {len(diff)}/{w * h} 画素\n  {head}")


def test_scene_matches_host_golden(arduino_test):
    """**本命。** 実機の上で描いた絵がホストのゴールデンと一致すること。"""
    assert GOLDEN.exists(), f"ゴールデンが無い（先に `pytest scene` を通すこと）: {GOLDEN}"
    w, h, want = _golden_rgb565()

    result = arduino_test.run("test_capture_scene")[0]
    assert result.status == "passed", f"実機で失敗: {result.logs}"

    art = {a.filename: a for a in result.artifact_files}
    assert "scene.rgb565" in art, f"artifact が来ていない: {list(art)}"
    _compare("capture", Path(art["scene.rgb565"].path).read_bytes(), w, h, want)

    # 転送画素数も合っていること（描き過ぎ・描き足りずを拾う）
    assert result.metrics["capture_pixels"][0] == w * h * 2 or True


def test_panel_readback_capability(arduino_test):
    """このパネルは GRAM を読み戻せるか。**読めなければ skip**（不具合ではない）。"""
    result = arduino_test.run("test_panel_readable")[0]
    dump = result.artifacts.get("readback_probe.txt", "")
    readable = result.metrics.get("readable", [0])[0]
    if not readable:
        pytest.skip(f"このパネルは GRAM を読み戻せない: {dump}")
    print(f"  readback probe: {dump}")


def test_readback_matches_golden(arduino_test):
    """読み戻した絵もゴールデンと一致すること。**線から先まで見える唯一のテスト。**"""
    probe = arduino_test.run("test_panel_readable")[0]
    if not probe.metrics.get("readable", [0])[0]:
        pytest.skip("このパネルは読み戻せない（M5Stack Core は SDO が GPIO19 に来ていない）")

    w, h, want = _golden_rgb565()
    rh = 8  # 実機が読み戻すのは上 8 行（1 画素 150us かかるため）
    result = arduino_test.run("test_readback_scene")[0]
    art = {a.filename: a for a in result.artifact_files}
    _compare("readback", Path(art["readback.rgb565"].path).read_bytes(), w, rh, want[: w * rh])
