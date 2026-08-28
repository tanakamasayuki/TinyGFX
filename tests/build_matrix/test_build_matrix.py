"""Tier 2 — 移植性のコンパイル検証。**実行しない。ビルドが通ることだけ見る。**

ホストで動かせない `TinyGFXBusSPI` / `TinyGFXBusSoftSPI` の型エラーや API 変更を
捕まえるのが目的。examples がそのまま素材になる。

examples は sketch.yaml にプロファイルを持っているので `--profile` でビルドする。
"""

import pytest

import tinygfx_build as tb

EXAMPLES = tb.REPO / "examples"
ALL = ["HelloWorld", "Shapes", "FlickerFree", "HardwareSPI", "OledI2C"]
# ボード専用の例。他のコアではビルドできない（ピンが決め打ち）
BOARD_ONLY = [("m5stack", "M5StackBasic")]

# プロファイル名 -> それが要求するコア。入っていなければ skip する
# （プロファイルは宣言したプラットフォームを勝手に入れないので、
#  ここで先に見ておかないと CI で巨大なコアを取りにいく事故になる）。
PROFILE_CORE = {
    "ch32v003": "ch32-riscv-arduino:ch32riscv:CH32V003_EVT",
    "uno": "arduino:avr:uno",
    "esp32": "esp32:esp32:esp32",
    "m5stack": "esp32:esp32:m5stack_core",
}

# CH32V003 のコアには SPI ライブラリが無いので HardwareSPI は外す
# （docs/EXTERNAL_REQUESTS.ja.md E2）。
CASES = (
    [("ch32v003", e) for e in ALL if e != "HardwareSPI"]
    + [("uno", e) for e in ALL]
    # esp32 はビルドが重いわりに他のコアで拾えない問題が少ないので 1 本だけ。
    + [("esp32", "HelloWorld")]
    # 実機立ち上げ用（docs/MANUAL_TEST.ja.md M0）。ILI9342 を通すのはここだけ。
    + BOARD_ONLY
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


def test_manual_sketch_builds():
    """実機検証スケッチ（`tests/manual/m5stack/`）が腐っていないこと。

    自動では走らせない（実機でしか意味がない）。**ビルドだけ通す。**
    docs/MANUAL_TEST.ja.md M0b。
    """
    if not tb.have_core(PROFILE_CORE["m5stack"]):
        pytest.skip("esp32 のコアが入っていない")
    build = tb.compile_profile(tb.REPO / "tests" / "manual" / "m5stack", "m5stack")
    assert build["flash"] is not None
    print(f"  manual     m5stack      flash={build['flash']:>7} ram={build['ram']}")


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


# フォントデコーダの切り落としマクロ（src/TinyGFX/FontCell.h）。
# 既定は全部 on。off の経路はスケッチが使わないとビルドされないので、
# ここで実際にコンパイルしておかないと静かに腐る。
FONT_MACROS = ["TINYGFX_FONT_BG", "TINYGFX_FONT_SCALE", "TINYGFX_FONT_CHAIN"]


@pytest.mark.parametrize("off", [[m] for m in FONT_MACROS] + [FONT_MACROS],
                         ids=lambda v: "+".join(m.split("_")[-1].lower() for m in v))
def test_font_macros_build(off):
    """フォントの切り落としマクロを 1 つずつと全部 off でビルドする。

    ついでに削減量を出す。**基準機（CH32V003）で測る**こと。AVR で効く
    最適化が RISC-V で逆効果になる例が出ている（docs/OPTIMIZE.ja.md J）。
    """
    if not tb.have_core(tb.CH32V003):
        pytest.skip("CH32V003 のコアが入っていない")
    base = tb.compile_construct("t")
    got = tb.compile_construct("t", defines={m: 0 for m in off})
    saved = base["flash"] - got["flash"]
    print(f"  {'+'.join(off):<58} -{saved} B")
    assert saved >= 0, f"{off} を off にして {-saved} B 増えている"
