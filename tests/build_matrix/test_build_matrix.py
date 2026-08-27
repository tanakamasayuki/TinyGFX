"""Tier 2 — 移植性のコンパイル検証。**実行しない。ビルドが通ることだけ見る。**

ホストで動かせない `TinyGFXBusSPI` / `TinyGFXBusSoftSPI` の型エラーや API 変更を
捕まえるのが目的。examples がそのまま素材になる。

examples は sketch.yaml にプロファイルを持っているので `--profile` でビルドする。
"""

import pytest

import tinygfx_build as tb

EXAMPLES = tb.REPO / "examples"
ALL = ["HelloWorld", "Shapes", "FlickerFree", "HardwareSPI"]

# プロファイル名 -> それが要求するコア。入っていなければ skip する
# （プロファイルは宣言したプラットフォームを勝手に入れないので、
#  ここで先に見ておかないと CI で巨大なコアを取りにいく事故になる）。
PROFILE_CORE = {
    "ch32v003": "ch32-riscv-arduino:ch32riscv:CH32V003_EVT",
    "uno": "arduino:avr:uno",
    "esp32": "esp32:esp32:esp32",
}

# CH32V003 のコアには SPI ライブラリが無いので HardwareSPI は外す
# （docs/EXTERNAL_REQUESTS.ja.md E2）。
CASES = (
    [("ch32v003", e) for e in ALL if e != "HardwareSPI"]
    + [("uno", e) for e in ALL]
    # esp32 はビルドが重いわりに他のコアで拾えない問題が少ないので 1 本だけ。
    + [("esp32", "HelloWorld")]
)

pytestmark = [
    pytest.mark.slow,
    pytest.mark.skipif(not tb.have_arduino_cli(), reason="arduino-cli がない"),
]


@pytest.mark.parametrize("profile,example", CASES, ids=lambda v: v)
def test_example_builds(profile, example):
    if not tb.have_core(PROFILE_CORE[profile]):
        pytest.skip(f"{PROFILE_CORE[profile]} のコアが入っていない")
    build = tb.compile_profile(EXAMPLES / example, profile)
    assert build["flash"] is not None, f"{example} @ {profile}: サイズが取れていない"
    print(f"  {profile:<10} {example:<12} flash={build['flash']:>7} ram={build['ram']}")


def test_hardware_spi_still_fails_on_ch32():
    """CH32V003 のコアで HardwareSPI が通らないことを**記録として**残す。

    TinyGFX の不具合ではなくコア側の状況（EXTERNAL_REQUESTS.ja.md E2）。
    通るようになったら CASES に足して、このテストは消す。
    """
    if not tb.have_core(PROFILE_CORE["ch32v003"]):
        pytest.skip("CH32 コアが入っていない")
    try:
        tb.compile_profile(EXAMPLES / "HardwareSPI", "ch32v003")
    except tb.BuildError as exc:
        assert "SPI.h" in str(exc), f"想定と違う失敗: {exc}"
        return
    pytest.skip("CH32 コアで SPI が使えるようになった。CASES に足してこのテストを消すこと")
